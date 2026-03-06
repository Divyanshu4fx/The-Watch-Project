#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_timer.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "lvgl.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "nvs_flash.h"
#include "pins.h"
#include "ui.h"

#define SCREEN_W 240
#define SCREEN_H 135
#define LCD_HOST SPI2_HOST

static const char *TAG = "BLE_CLOCK";

static bool ble_connected = false;
static uint8_t own_addr_type;

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc_cali_handle;

void battery_adc_init(void) {
    // 1. Initialize ADC Unit 1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    // 2. Configure GPIO38 (ADC1 Channel 2)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));

    // 3. Initialize Hardware Calibration (Reads factory eFuses)
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));
}

int get_battery_percent(void) {
    int raw_val = 0;
    int sample;
    int voltage_mv;
    
    // Take 10 quick samples to average out immediate hardware noise
    for(int i = 0; i < 10; i++) {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &sample);
        raw_val += sample;
    }
    raw_val /= 10;
    
    adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv);
    float battery_voltage = (voltage_mv * 2.0) / 1000.0;

    // Calculate the raw, jumping percentage
    int raw_pct;
    if (battery_voltage >= 4.15) raw_pct = 100;
    else if (battery_voltage <= 3.30) raw_pct = 0;
    else raw_pct = (int)((battery_voltage - 3.30) / (4.15 - 3.30) * 100);

    // --- THE SMOOTHING FILTER ---
    // static means this variable remembers its value between function calls
    static float smoothed_pct = -1.0; 
    
    if (smoothed_pct == -1.0) {
        // On the very first boot, trust the raw reading immediately
        smoothed_pct = raw_pct; 
    } else {
        // Blend 95% of the old stable value with only 5% of the new jumping value
        smoothed_pct = (smoothed_pct * 0.95) + (raw_pct * 0.05);
    }

    return (int)smoothed_pct;
}

bool is_charging(void) {
    int raw_val = 0;
    int sample;
    int voltage_mv;
    
    // Take 10 samples to prevent false spikes
    for(int i = 0; i < 10; i++) {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &sample);
        raw_val += sample;
    }
    raw_val /= 10;
    
    adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv);

    // Calculate actual battery voltage
    float battery_voltage = (voltage_mv * 2.0) / 1000.0;

    // If voltage is artificially high, the charger is actively running
    if (battery_voltage >= 4.18) {
        return true;
    }
    
    return false;
}
/* ================= BLE UUIDs ================= */

static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,
                     0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0);

static const ble_uuid128_t time_char_uuid =
    BLE_UUID128_INIT(0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd,
                     0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd);

char *num_to_week(uint8_t weekday)
{
    switch (weekday)
    {
    case 0:
        return "Sunday";
    case 1:
        return "Monday";
    case 2:
        return "Tuesday";
    case 3:
        return "Wednesday";
    case 4:
        return "Thursday";
    case 5:
        return "Friday";
    case 6:
        return "Saturday";
    default:
        return "";
    }
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver); // NOW we tell LVGL it is safe to continue
    return false;
}

// The LVGL flush callback
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    // 1. Retrieve your esp_lcd panel handle from LVGL's user_data
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;

    // 2. Push the color data to the SPI bus
    // Note: esp_lcd expects the end coordinates to be outside the active area (+1)
    esp_lcd_panel_draw_bitmap(panel,
                              area->x1,
                              area->y1,
                              area->x2 + 1,
                              area->y2 + 1,
                              (void *)color_map);

    // 3. IMPORTANT: Tell LVGL you are done!
    // If you forget this line, LVGL will freeze forever waiting for the SPI transfer to finish.
    // lv_disp_flush_ready(drv);
}

/* ================= BLE HANDLER ================= */

static int time_write_handler(uint16_t conn_handle,
                              uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt,
                              void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        ctxt->om->om_len == 4)
    {

        uint32_t unix_time;
        memcpy(&unix_time, ctxt->om->om_data, 4);

        struct timeval tv = {
            .tv_sec = unix_time,
            .tv_usec = 0};
        settimeofday(&tv, NULL);

        ESP_LOGI(TAG, "Time synced: %lu", unix_time);
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &time_char_uuid.u,
                .access_cb = time_write_handler,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {0}},
    },
    {0}};

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Device connected! Status: %d", event->connect.status);
        ble_connected = (event->connect.status == 0);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Device disconnected, restarting advertising...");
        ble_connected = false;

        // Restart advertising
        struct ble_gap_adv_params adv_params;
        memset(&adv_params, 0, sizeof(adv_params));
        adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

        ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                          &adv_params, ble_gap_event, NULL);
        break;
    }
    return 0;
}

static void ble_on_sync(void)
{
    esp_err_t rc;
    ble_hs_id_infer_auto(0, &own_addr_type);

    // 2. Set the device name
    ble_svc_gap_device_name_set("M5_Watch");

    // 3. Configure advertisement fields
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error setting advertisement fields; rc=%d", rc);
        return;
    }

    // 4. Start advertising
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error starting advertising; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "Device synced and advertising started as M5_Watch!");
}

void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_init(void)
{

    esp_nimble_hci_init();
    esp_err_t ret;

    // Initialize the NimBLE port
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "NimBLE port init failed: %d", ret);
        return;
    }

    // Configure standard GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set up our custom GATT services
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0)
        ESP_LOGE(TAG, "GATT count error: %d", rc);

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0)
        ESP_LOGE(TAG, "GATT add error: %d", rc);

    // Register the sync callback (Fires when Host and Controller are ready)
    ble_hs_cfg.sync_cb = ble_on_sync;

    // Start the FreeRTOS task for NimBLE
    nimble_port_freertos_init(ble_host_task);
}

// Tells LVGL that time has passed (called every 2 milliseconds)
static void lv_tick_task(void *arg)
{
    lv_tick_inc(2);
}

/* ================= CLOCK TASK ================= */

void clock_task(void *pvParameters)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = M5_TFT_SCLK_PIN,
        .mosi_io_num = M5_TFT_MOSI_PIN,
        .miso_io_num = -1,
        .max_transfer_sz = SCREEN_W * SCREEN_H * 2,
    };
    spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = M5_TFT_DC_PIN,
        .cs_gpio_num = M5_TFT_CS_PIN,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io);

    // We declare disp_drv here so we can pass it to the callback
    static lv_disp_drv_t disp_drv;

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    // Attach the callback to the SPI IO handle
    esp_lcd_panel_io_register_event_callbacks(io, &cbs, &disp_drv);

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = M5_TFT_RST_PIN,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io, &panel_config, &panel);

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_set_gap(panel, 40, 53);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_swap_xy(panel, true);
    esp_lcd_panel_mirror(panel, true, false);
    esp_lcd_panel_disp_on_off(panel, true);

    gpio_set_direction(M5_TFT_BACKLIGHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(M5_TFT_BACKLIGHT_PIN, 1);

    lv_init();

    /* --- 1.5 Start the LVGL Tick Timer --- */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"};

    /* --- 2. Allocate a DMA-capable draw buffer --- */
    // Instead of a static array, we explicitly allocate it in DMA RAM
    size_t buffer_size = SCREEN_W * 14 * sizeof(lv_color_t);
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_W * 14);

    esp_timer_handle_t lvgl_tick_timer;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000); // 2 milliseconds (in microseconds)

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W;
    disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel; // esp_lcd display handle
    lv_disp_drv_register(&disp_drv);

    /* Initialize EEZ Studio UI */
    ui_init();
    lv_label_set_text(objects.battery_percentage_label, "99");
    char time_str[16];
    char date_str[20];
    int last_sec = -1; // Used to prevent unnecessary screen updates
    while (1)
    {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Only update the UI text if the second has actually changed
        if (timeinfo.tm_sec != last_sec)
        {
            last_sec = timeinfo.tm_sec;

            uint8_t hour = timeinfo.tm_hour;
            bool is_pm = (hour >= 12);
            uint8_t weekday = timeinfo.tm_wday;

            // Convert 24-hour to 12-hour format properly
            if (hour > 12)
            {
                hour = hour - 12;
            }
            else if (hour == 0)
            {
                hour = 12; // Handle midnight
            }

            // Update AM/PM Label
            if (is_pm)
            {
                lv_label_set_text(objects.am_pm, "PM");
            }
            else
            {
                lv_label_set_text(objects.am_pm, "AM");
            }

            // Update Time Label
            sprintf(time_str, "%02d:%02d:%02d", hour, timeinfo.tm_min, timeinfo.tm_sec);
            lv_label_set_text(objects.time_lable, time_str);

            sprintf(date_str, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year + 1900);
            lv_label_set_text(objects.date_label, date_str);

            lv_label_set_text(objects.obj2, num_to_week(weekday));

            // Update BLE Status Label Visibility
            if (ble_connected)
            {
                lv_obj_clear_flag(objects.bluetooth_icon, LV_OBJ_FLAG_HIDDEN); // Show it
            }
            else
            {
                lv_obj_add_flag(objects.bluetooth_icon, LV_OBJ_FLAG_HIDDEN); // Hide it
            }

            if(is_charging())
            {
                lv_obj_set_style_text_color(objects.battery_percentage_label,lv_color_hex(0x00FF00), 0);
            }
            else
            {
                lv_obj_set_style_text_color(objects.battery_percentage_label,lv_color_hex(0x000000), 0);
            }

            char buf[4];
            snprintf(buf, sizeof(buf), "%d", get_battery_percent());
            lv_label_set_text(objects.battery_percentage_label, buf);
        }

        // Let LVGL process the changes and draw to the screen
        lv_timer_handler();

        // 10ms delay is standard for LVGL. It keeps the UI highly responsive
        // without hogging the CPU from your BLE stack.
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // /* --- Set Screen Background to Black --- */
    // lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    // /* --- 4. Create your UI Objects --- */

    // // 1. Time Label
    // lv_obj_t *time_label = lv_label_create(lv_scr_act());
    // lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0); // Make text white
    // lv_obj_set_style_text_font(time_label, &lv_font_montserrat_40, 0);
    // // Align it to the center, but shift it slightly left (-15px) to make room for AM/PM
    // lv_obj_align(time_label, LV_ALIGN_CENTER, -15, 0);

    // // 2. AM/PM Label
    // lv_obj_t *ampm_label = lv_label_create(lv_scr_act());
    // lv_obj_set_style_text_font(ampm_label, &lv_font_montserrat_24, 0);
    // lv_obj_set_style_text_color(ampm_label, lv_color_hex(0xFF0000), 0); // Red color
    // // Automatically position this label to the right of the time_label
    // lv_obj_align_to(ampm_label, time_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 45, -5);

    // // 3. BLE Status Label
    // lv_obj_t *ble_label = lv_label_create(lv_scr_act());
    // lv_label_set_text(ble_label, "1");
    // lv_obj_set_style_text_color(ble_label, lv_color_hex(0xFF0000), 0); // Red color
    // lv_obj_align(ble_label, LV_ALIGN_TOP_LEFT, 5, 5); // Put in top left corner
    // lv_obj_add_flag(ble_label, LV_OBJ_FLAG_HIDDEN);   // Hide it by default

    // /* --- 5. The LVGL Task Loop --- */
    // char time_str[16];
    // int last_sec = -1; // Used to prevent unnecessary screen updates

    // while (1) {
    //     time_t now;
    //     struct tm timeinfo;
    //     time(&now);
    //     localtime_r(&now, &timeinfo);

    //     // Only update the UI text if the second has actually changed
    //     if (timeinfo.tm_sec != last_sec) {
    //         last_sec = timeinfo.tm_sec;

    //         uint8_t hour = timeinfo.tm_hour;
    //         bool is_pm = (hour >= 12);

    //         // Convert 24-hour to 12-hour format properly
    //         if (hour > 12) {
    //             hour = hour - 12;
    //         } else if (hour == 0) {
    //             hour = 12; // Handle midnight
    //         }

    //         // Update AM/PM Label
    //         if (is_pm) {
    //             lv_label_set_text(ampm_label, "PM");
    //         } else {
    //             lv_label_set_text(ampm_label, "AM");
    //         }

    //         // Update Time Label
    //         sprintf(time_str, "%02d:%02d:%02d", hour, timeinfo.tm_min, timeinfo.tm_sec);
    //         lv_label_set_text(time_label, time_str);

    //         // Update BLE Status Label Visibility
    //         if (ble_connected) {
    //             lv_obj_clear_flag(ble_label, LV_OBJ_FLAG_HIDDEN); // Show it
    //         } else {
    //             lv_obj_add_flag(ble_label, LV_OBJ_FLAG_HIDDEN);   // Hide it
    //         }
    //     }

    //     // Let LVGL process the changes and draw to the screen
    //     lv_timer_handler();

    //     // 10ms delay is standard for LVGL. It keeps the UI highly responsive
    //     // without hogging the CPU from your BLE stack.
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

    // while (1) {
    //     time_t now;
    //     struct tm timeinfo;
    //     time(&now);
    //     localtime_r(&now, &timeinfo);

    //     memset(buffer, 0x00, SCREEN_W * SCREEN_H * 2);

    //     uint8_t hour = timeinfo.tm_hour;
    //     if(hour > 12 )
    //     {
    //         draw_string(buffer,200,80,"PM",0xF800,2);
    //         hour = hour - 12;
    //     }
    //     else
    //         draw_string(buffer,200,80,"AM",0xF800,2);

    //     sprintf(time_str, "%02d:%02d:%02d",
    //             hour,
    //             timeinfo.tm_min,
    //             timeinfo.tm_sec);

    //     draw_string(buffer, 30, 45, time_str, 0x07FF, 4);

    //     if (ble_connected) {
    //         // Draw a red "1" in the top left if a BLE device is currently connected
    //         draw_string(buffer, 5, 5, "1", 0xF800, 2);
    //     }

    //     esp_lcd_panel_draw_bitmap(panel, 0, 0, SCREEN_W, SCREEN_H, buffer);

    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}

/* ================= MAIN ================= */

void app_main(void)
{
    // NVS is required for BLE to operate correctly (even if not saving bonding data)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    gpio_reset_pin(M5_POWER_HOLD_PIN);
    gpio_set_direction(M5_POWER_HOLD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(M5_POWER_HOLD_PIN, 1);

    setenv("TZ", "IST-5:30", 1);
    tzset();

    battery_adc_init();
    ble_init();

    xTaskCreatePinnedToCore(clock_task,
                            "ClockTask",
                            8192,
                            NULL,
                            5,
                            NULL,
                            1);
}
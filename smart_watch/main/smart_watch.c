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

#include "lvgl.h"

#include "nvs_flash.h"
#include "pins.h"
#include "ui.h"
#include "battery.h"
#include "ble.h"
#include "battery.h"
#include "clock.h"
#include "buttons.h"
#include "notification.h"
#include "ir_blaster.h"

// System Stats
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_private/esp_clk.h"

#define SCREEN_W 240
#define SCREEN_H 135
#define LCD_HOST SPI2_HOST

static const char *TAG = "APP_MAIN";

volatile bool screen_on = true;
volatile uint32_t last_activity_time = 0;
bool alarm_popup_enabled = false;


void enable_alarm_popup(uint8_t hour, uint8_t minute, char *message);

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

// Tells LVGL that time has passed (called every 2 milliseconds)
static void lv_tick_task(void *arg)
{
    lv_tick_inc(2);
}

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

    last_activity_time = esp_log_timestamp();

    // ── Fake notifications for testing ──
    // add_notification("WhatsApp", "John: Hey!", "Hey, are you free today? Want to grab coffee later? I'm free after 4pm.", 9, 32);
    // add_notification("Gmail", "Meeting Reminder", "Your 3:00 PM meeting with the design team starts in 30 minutes. Conference Room B.", 9, 15);
    // add_notification("Slack", "#general - Alice", "New deployment scheduled for tonight at 11pm. Please review the changelog.", 8, 45);
    // add_notification("Instagram", "New follower", "techguru42 started following you.", 8, 30);
    // add_notification("Phone", "Missed Call", "Missed call from Mom at 7:55 AM", 7, 55);
    // enable_alarm_popup(12, 0, "Testing");
    while (1)
    {
        uint32_t current_time = esp_log_timestamp();
        if (screen_on && (current_time - last_activity_time > 10000) && !(alarm_popup_enabled || notification_screen_active))
        {
            screen_on = false;
            // Turn off backlight
            gpio_set_level(M5_TFT_BACKLIGHT_PIN, 0);
            ESP_LOGI(TAG, "Screen turned off due to inactivity");
        }

        check_alarms();
        get_battery_percent();

        if (alarm_popup_enabled || notification_screen_active)
            screen_on = true;
        if (screen_on)
        {
            // Turn backlight on (in case it just woke up)
            gpio_set_level(M5_TFT_BACKLIGHT_PIN, 1);

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

                lv_label_set_text(objects.week_label, num_to_week(weekday));

                // Update BLE Status Label Visibility
                if (is_ble_connected())
                {
                    lv_obj_clear_flag(objects.bluetooth_icon, LV_OBJ_FLAG_HIDDEN); // Show it
                }
                else
                {
                    lv_obj_add_flag(objects.bluetooth_icon, LV_OBJ_FLAG_HIDDEN); // Hide it
                }

                if (is_any_alarm_enabled)
                {
                    lv_obj_clear_flag(objects.alarm_icon, LV_OBJ_FLAG_HIDDEN); // Show it
                }
                else
                {
                    lv_obj_add_flag(objects.alarm_icon, LV_OBJ_FLAG_HIDDEN); // Hide it
                }
                char buf[5];
                snprintf(buf, sizeof(buf), "%d%%", get_battery_percent());
                lv_label_set_text(objects.battery_percentage_label, buf);
            }

            // Let LVGL process the changes and draw to the screen
            lv_timer_handler();
        }

        // 10ms delay is standard for LVGL. It keeps the UI highly responsive
        // without hogging the CPU from your BLE stack.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void system_info_task(void *pvParameters)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;

    /* ---- PRINT ONCE ---- */
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "Cores: %d", chip_info.cores);
    // ESP_LOGI(TAG, "CPU freq: %lu MHz", esp_cpu_get_freq_mhz());
    ESP_LOGI(TAG, "CPU freq: %d MHz", esp_clk_cpu_freq() / 1000000);
    ESP_LOGI(TAG, "Total RAM: %d KB",
             heap_caps_get_total_size(MALLOC_CAP_DEFAULT) / 1024);
    ESP_LOGI(TAG, "Flash: %lu MB", flash_size / (1024 * 1024));

    /* ---- PRINT CONTINUOUSLY ---- */
    while (1)
    {
        ESP_LOGI(TAG, "Available RAM: %lu KB",
                 esp_get_free_heap_size() / 1024);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    xTaskCreate(system_info_task, "sys_info", 2048, NULL, 0, NULL);
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

    buzzer_init();
    // buzzer_on();
    // vTaskDelay(pdMS_TO_TICKS(200));
    // buzzer_off();

    input_manager_init();
    load_alarms_from_nvs();
    battery_adc_init();
    notifications_init();
    ble_init();
    // i2c_init();
    rtc_initialize();
    get_time_from_rtc();
    init_ir_blaster();

    xTaskCreatePinnedToCore(clock_task,
                            "ClockTask",
                            8192,
                            NULL,
                            5,
                            NULL,
                            1);
}

void enable_alarm_popup(uint8_t hour, uint8_t minute, char *message)
{
    if (alarm_popup_enabled)
        return;

    // Allocate 6 bytes: 2 for hour + 1 for colon + 2 for minute + 1 for null terminator
    char buf[10] = {0};
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);

    lv_label_set_text(objects.alarm_time, buf);
    if (message != NULL)
    {
        lv_label_set_text(objects.message_box, message);

        // Set text alignment to center
        lv_obj_set_style_text_align(objects.message_box, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_align(objects.message_box, LV_ALIGN_BOTTOM_MID, 0, 0);

        // Enable circular auto-scrolling for text that exceeds the object's bounding box width
        lv_label_set_long_mode(objects.message_box, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }

    // Show the popup container
    lv_obj_clear_flag(objects.alarm_container, LV_OBJ_FLAG_HIDDEN);

    alarm_popup_enabled = true;
}

void disable_alarm_popup()
{
    if (!alarm_popup_enabled)
        return;

    // Hide the popup container
    lv_obj_add_flag(objects.alarm_container, LV_OBJ_FLAG_HIDDEN);

    alarm_popup_enabled = false;
}
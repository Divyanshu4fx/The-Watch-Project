#include "ble.h"

static const char *TAG = "BL_TASK";

static bool ble_connected = false;
static uint8_t own_addr_type;
static uint16_t ble_conn_handle = 0;

// extern myalarm_t alarms[MAX_ALARMS]; // defined in clock.c
// extern uint16_t alarm_notify_handle;

/* ================= BLE UUIDs ================= */

static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,
                     0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0);

static const ble_uuid128_t time_char_uuid =
    BLE_UUID128_INIT(0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd,
                     0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd);

static const ble_uuid128_t alarm_char_uuid =
    BLE_UUID128_INIT(0xe7, 0x73, 0xcc, 0x76, 0xc4, 0x69, 0x45, 0x1e,
                     0x8a, 0xc5, 0x10, 0xc0, 0x38, 0xc8, 0xee, 0xed);

static const ble_uuid128_t findMyWatch_char_uuid =
    BLE_UUID128_INIT(0x87, 0x2e, 0x03, 0xad, 0x3d, 0x5d, 0x4b, 0x79,
                     0x95, 0x71, 0x05, 0x8e, 0xea, 0x06, 0x6e, 0x3e);

static void notify_alarm_to_app(const myalarm_t *alarm)
{
    if (!ble_connected)
        return;

    uint8_t msg_len = strlen((char *)alarm->message);
    uint8_t buf[4 + MAX_MSG_LEN];
    buf[0] = alarm->index;
    buf[1] = alarm->hour;
    buf[2] = alarm->minute;
    buf[3] = alarm->enabled;
    memcpy(&buf[4], alarm->message, msg_len);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, 4 + msg_len);
    ble_gatts_notify_custom(ble_conn_handle, alarm_notify_handle, om);
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

        set_time_on_rtc();

        ESP_LOGI(TAG, "Time synced: %lu", unix_time);
    }
    return 0;
}

static int alarm_handler(uint16_t conn_handle,
                         uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt,
                         void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        return 0;
    }

    uint8_t *data = ctxt->om->om_data;
    uint16_t len = ctxt->om->om_len;

    // Request all alarms: single byte 0xFF
    if (len == 1 && data[0] == 0xFF)
    {
        ESP_LOGI(TAG, "Alarm fetch requested, sending all alarms");
        for (int i = 0; i < MAX_ALARMS; i++)
        {
            if (alarms[i].hour != 0 || alarms[i].minute != 0 || strlen((char *)alarms[i].message) > 0)
            {
                notify_alarm_to_app(&alarms[i]);
            }
        }
        return 0;
    }

    // Set alarm: [index:1][hour:1][minute:1][enabled:1][message:N]
    if (len >= 4)
    {
        uint8_t index = data[0];
        if (index >= MAX_ALARMS)
        {
            ESP_LOGW(TAG, "Alarm index %d out of range", index);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        alarms[index].index = index;
        alarms[index].hour = data[1];
        alarms[index].minute = data[2];
        alarms[index].enabled = data[3];

        memset(alarms[index].message, 0, MAX_MSG_LEN);
        if (len > 4)
        {
            uint8_t msg_len = (len - 4) < MAX_MSG_LEN ? (len - 4) : (MAX_MSG_LEN - 1);
            memcpy(alarms[index].message, &data[4], msg_len);
        }

        ESP_LOGI(TAG, "Alarm[%d] set: %02d:%02d enabled=%d msg='%s'",
                 index, alarms[index].hour, alarms[index].minute,
                 alarms[index].enabled, alarms[index].message);

        // Save to NVS for persistence across reboots
        save_alarm_to_nvs(index);
    }

    return 0;
}

static int find_my_watch_handler(uint16_t conn_handle,
                                 uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt,
                                 void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        ctxt->om->om_len == 1)
    {
        uint8_t command = ctxt->om->om_data[0];

        if (command == 0x01)
        {
            ESP_LOGI(TAG, "Find My Watch: BUZZ ON");
            buzzer_on();
        }
        else
        {
            ESP_LOGI(TAG, "Find My Watch: BUZZ OFF");
            buzzer_off();
        }
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
            {
                .uuid = &alarm_char_uuid.u,
                .access_cb = alarm_handler,
                .val_handle = &alarm_notify_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &findMyWatch_char_uuid.u,
                .access_cb = find_my_watch_handler,
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
        if (ble_connected) {
            ble_conn_handle = event->connect.conn_handle;
        }
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

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event: handle=%d, cur_notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
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

bool is_ble_connected(void)
{
    return ble_connected;
}
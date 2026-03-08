#include "clock.h"

static const char *TAG = "RTC";

// Forward declarations pointing to the definitions in the main file
extern void enable_alarm_popup(uint8_t hour, uint8_t minute, char * message);
extern void disable_alarm_popup(void);

// I2C pins for the RTC
#define I2C_PORT I2C_NUM_0
#define SDA_GPIO M5_I2C_SDA_PIN
#define SCL_GPIO M5_I2C_SCL_PIN

static i2c_dev_t rtc_handle;

// Define the timing pattern in milliseconds
static const uint16_t ALARM_PATTERN_MS[] = {100, 100, 100, 500}; 
// Define the corresponding buzzer state (1 = ON, 0 = OFF)
static const uint8_t ALARM_STATE[] = {1, 0, 1, 0}; 
static const uint8_t PATTERN_LENGTH = 4;

myalarm_t alarms[MAX_ALARMS] = {0};
uint16_t alarm_notify_handle;

// Alarm state tracking
alarm_state_t current_alarm_state = ALARM_STATE_IDLE;
int8_t ringing_alarm_index = -1;
bool is_any_alarm_enabled = false;

// Track which minute we last triggered each alarm to avoid re-triggering
static int8_t last_triggered_minute[MAX_ALARMS] = {-1, -1, -1, -1, -1};

// Snooze tracking
static uint8_t snooze_hour = 0;
static uint8_t snooze_minute = 0;
static bool snooze_active = false;

// Auto-stop: ring for max 60 seconds
static int64_t ring_start_time = 0;
#define ALARM_RING_DURATION_MS (60 * 1000)

static uint8_t current_step = 0;
static int64_t last_step_time = 0;

void rtc_initialize(void)
{
    i2cdev_init();

    pcf8563_init_desc(&rtc_handle, I2C_PORT, SDA_GPIO, SCL_GPIO);
}

void get_time_from_rtc(void)
{
    struct tm rtc_time;
    bool valid = false;

    if (pcf8563_get_time(&rtc_handle, &rtc_time, &valid) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read time from RTC");
        return;
    }
    if (!valid)
    {
        ESP_LOGE("TAG", "Time not Valid");
        return;
    }
    time_t now = mktime(&rtc_time);

    struct timeval tv = {
        .tv_sec = now,
        .tv_usec = 0};

    // Set ESP32 system time
    settimeofday(&tv, NULL);

    ESP_LOGI("TAG", "System time updated from RTC");
}

esp_err_t set_time_on_rtc(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    esp_err_t err = pcf8563_set_time(&rtc_handle, &timeinfo);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to set time in RTC");
    return err;
}

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

void buzzer_init() {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 4000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = M5_BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0
    };
    ledc_channel_config(&channel);
}

void buzzer_on() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void buzzer_off() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void alarm_update() {
    int64_t now = esp_timer_get_time() / 1000; // Get current time in ms

    // Evaluate if the duration for the current step has elapsed
    if (now - last_step_time < ALARM_PATTERN_MS[current_step]) {
        return; // Exit if the current interval is not yet finished
    }

    // Advance the state machine
    last_step_time = now;
    current_step = (current_step + 1) % PATTERN_LENGTH;

    // 4000 Hz replicates the standard high-pitch piezo resonance of classic digital watches
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 4000); 

    if (ALARM_STATE[current_step] == 1) {
        // Execute Beep (50% duty cycle)
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        // Execute Silence (0% duty cycle)
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void alarm_stop() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ================= NVS ALARM PERSISTENCE ================= */

esp_err_t save_alarm_to_nvs(uint8_t index)
{
    if (index >= MAX_ALARMS)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    // Build key like "alarm_0", "alarm_1", etc.
    char key[12];
    snprintf(key, sizeof(key), "alarm_%d", index);

    // Store the entire alarm struct as a blob
    err = nvs_set_blob(handle, key, &alarms[index], sizeof(myalarm_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS set_blob failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Alarm[%d] saved to NVS: %02d:%02d enabled=%d msg='%s'",
                 index, alarms[index].hour, alarms[index].minute,
                 alarms[index].enabled, alarms[index].message);
    }

    nvs_close(handle);
    return err;
}

esp_err_t load_alarms_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No alarms stored in NVS yet");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    for (int i = 0; i < MAX_ALARMS; i++)
    {
        char key[12];
        snprintf(key, sizeof(key), "alarm_%d", i);

        size_t required_size = sizeof(myalarm_t);
        err = nvs_get_blob(handle, key, &alarms[i], &required_size);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded alarm[%d]: %02d:%02d enabled=%d msg='%s'",
                     i, alarms[i].hour, alarms[i].minute,
                     alarms[i].enabled, alarms[i].message);
            if(alarms[i].enabled)
                is_any_alarm_enabled = true;
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            // No alarm at this index, leave as zero
            memset(&alarms[i], 0, sizeof(myalarm_t));
            alarms[i].index = i;
        }
        else
        {
            ESP_LOGE(TAG, "NVS get_blob alarm_%d failed: %s", i, esp_err_to_name(err));
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t delete_alarm_from_nvs(uint8_t index)
{
    if (index >= MAX_ALARMS)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    char key[12];
    snprintf(key, sizeof(key), "alarm_%d", index);

    err = nvs_erase_key(handle, key);
    if (err == ESP_OK)
    {
        nvs_commit(handle);
        memset(&alarms[index], 0, sizeof(myalarm_t));
        alarms[index].index = index;
        ESP_LOGI(TAG, "Alarm[%d] deleted from NVS", index);
    }

    nvs_close(handle);
    return err;
}

/* ================= ALARM CHECKER ================= */

// Call this every second from your main loop / UI task
void check_alarms(void)
{
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    uint8_t cur_hour = timeinfo.tm_hour;
    uint8_t cur_min = timeinfo.tm_min;

    // If currently ringing, keep updating the buzzer pattern
    if (current_alarm_state == ALARM_STATE_RINGING)
    {
        alarm_update();

        // Auto-stop after ALARM_RING_DURATION_MS
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - ring_start_time > ALARM_RING_DURATION_MS)
        {
            ESP_LOGW(TAG, "Alarm[%d] auto-dismissed after timeout", ringing_alarm_index);
            dismiss_alarm();
        }
        return;
    }

    // Check snooze
    if (snooze_active && cur_hour == snooze_hour && cur_min == snooze_minute)
    {
        enable_alarm_popup(snooze_hour,snooze_minute,NULL);
        ESP_LOGI(TAG, "Snooze alarm triggered at %02d:%02d", cur_hour, cur_min);
        snooze_active = false;
        current_alarm_state = ALARM_STATE_RINGING;
        ring_start_time = esp_timer_get_time() / 1000;
        return;
    }

    is_any_alarm_enabled = false;
    // Check each alarm
    for (int i = 0; i < MAX_ALARMS; i++)
    {
        if (!alarms[i].enabled)
            continue;
         is_any_alarm_enabled = true;
        if (alarms[i].hour == cur_hour && alarms[i].minute == cur_min)
        {
            // Avoid re-triggering in the same minute
            if (last_triggered_minute[i] == cur_min)
                continue;

            last_triggered_minute[i] = cur_min;
            ringing_alarm_index = i;
            current_alarm_state = ALARM_STATE_RINGING;
            ring_start_time = esp_timer_get_time() / 1000;

            ESP_LOGI(TAG, "*** ALARM[%d] TRIGGERED: %02d:%02d msg='%s' ***",
                     i, alarms[i].hour, alarms[i].minute, alarms[i].message);
            enable_alarm_popup(alarms[i].hour,alarms[i].minute,(char *)alarms[i].message);
            return;
        }
        else
        {
            // Reset trigger guard when minute changes
            if (last_triggered_minute[i] == cur_min)
                continue;
            last_triggered_minute[i] = -1;
        }
    }
}

void dismiss_alarm(void)
{
    if (current_alarm_state == ALARM_STATE_RINGING || current_alarm_state == ALARM_STATE_SNOOZED)
    {
        ESP_LOGI(TAG, "Alarm[%d] dismissed", ringing_alarm_index);
        disable_alarm_popup();
        alarm_stop();
        current_alarm_state = ALARM_STATE_IDLE;
        ringing_alarm_index = -1;
        snooze_active = false;
    }
}

void snooze_alarm(uint8_t snooze_minutes)
{
    if (current_alarm_state != ALARM_STATE_RINGING)
        return;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Calculate snooze time
    uint16_t total_min = timeinfo.tm_hour * 60 + timeinfo.tm_min + snooze_minutes;
    snooze_hour = (total_min / 60) % 24;
    snooze_minute = total_min % 60;
    snooze_active = true;

    alarm_stop();
    current_alarm_state = ALARM_STATE_SNOOZED;
    disable_alarm_popup();
    ESP_LOGI(TAG, "Alarm[%d] snoozed for %d min, will ring at %02d:%02d",
             ringing_alarm_index, snooze_minutes, snooze_hour, snooze_minute);
}
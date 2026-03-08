#include "clock.h"

static const char *TAG = "RTC";

// I2C pins for the RTC
#define I2C_PORT I2C_NUM_0
#define SDA_GPIO M5_I2C_SDA_PIN
#define SCL_GPIO M5_I2C_SCL_PIN

static i2c_dev_t rtc_handle;

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
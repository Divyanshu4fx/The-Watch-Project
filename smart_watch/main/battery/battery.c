#include "battery.h"

static const char *TAG = "BATTERY";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc_cali_handle;

void battery_adc_init(void)
{
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

int get_battery_percent(void)
{
    int raw_val = 0;
    int sample;
    int voltage_mv;

    // Take 10 quick samples to average out immediate hardware noise
    for (int i = 0; i < 10; i++)
    {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &sample);
        raw_val += sample;
    }
    raw_val /= 10;

    adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv);
    float battery_voltage = (voltage_mv * 2.0) / 1000.0;

    // Calculate the raw, jumping percentage
    int raw_pct;
    if (battery_voltage >= 4.15)
        raw_pct = 100;
    else if (battery_voltage <= 3.30)
        raw_pct = 0;
    else
        raw_pct = (int)((battery_voltage - 3.30) / (4.15 - 3.30) * 100);

    // --- THE SMOOTHING FILTER ---
    // static means this variable remembers its value between function calls
    static float smoothed_pct = -1.0;

    if (smoothed_pct == -1.0)
    {
        // On the very first boot, trust the raw reading immediately
        smoothed_pct = raw_pct;
    }
    else
    {
        // Blend 95% of the old stable value with only 5% of the new jumping value
        smoothed_pct = (smoothed_pct * 0.95) + (raw_pct * 0.05);
    }

    return (int)smoothed_pct;
}

bool is_charging(void)
{
    int raw_val = 0;
    int sample;
    int voltage_mv;

    // Take 10 samples to prevent false spikes
    for (int i = 0; i < 10; i++)
    {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &sample);
        raw_val += sample;
    }
    raw_val /= 10;

    adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv);

    // Calculate actual battery voltage
    float battery_voltage = (voltage_mv * 2.0) / 1000.0;

    // If voltage is artificially high, the charger is actively running
    if (battery_voltage >= 4.18)
    {
        return true;
    }

    return false;
}

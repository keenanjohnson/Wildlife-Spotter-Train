#pragma once

// Battery voltage monitoring for XIAO ESP32-S3
// Uses ADC_BAT pin (GPIO10 / ADC1_CHANNEL_9)

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

// XIAO ESP32-S3 battery ADC configuration
// GPIO10 maps to ADC1_CHANNEL_9
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_9
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12  // Full scale ~3.3V

// Voltage divider ratio (XIAO uses 1:2 divider on battery input)
// Adjust this if readings are off - measure with multimeter to calibrate
#define BATTERY_VOLTAGE_DIVIDER 2.0f

// Li-ion battery voltage thresholds
#define BATTERY_VOLTAGE_FULL    4.2f
#define BATTERY_VOLTAGE_EMPTY   3.0f

static const char *BATTERY_TAG = "BATTERY";

static adc_oneshot_unit_handle_t battery_adc_handle = NULL;
static adc_cali_handle_t battery_cali_handle = NULL;
static bool battery_calibrated = false;

// Initialize battery ADC
static esp_err_t battery_init(void) {
    esp_err_t ret;

    // Initialize ADC unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&unit_cfg, &battery_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(BATTERY_TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(battery_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(BATTERY_TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Try to create calibration handle for more accurate readings
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &battery_cali_handle);
    if (ret == ESP_OK) {
        battery_calibrated = true;
        ESP_LOGI(BATTERY_TAG, "ADC calibration enabled (curve fitting)");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &battery_cali_handle);
    if (ret == ESP_OK) {
        battery_calibrated = true;
        ESP_LOGI(BATTERY_TAG, "ADC calibration enabled (line fitting)");
    }
#endif

    if (!battery_calibrated) {
        ESP_LOGW(BATTERY_TAG, "ADC calibration not available, using raw values");
    }

    ESP_LOGI(BATTERY_TAG, "Battery monitoring initialized on GPIO10 (ADC1_CH9)");
    return ESP_OK;
}

// Read battery voltage in volts
static float battery_read_voltage(void) {
    if (battery_adc_handle == NULL) {
        return 0.0f;
    }

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(battery_adc_handle, BATTERY_ADC_CHANNEL, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(BATTERY_TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return 0.0f;
    }

    float voltage;
    if (battery_calibrated && battery_cali_handle != NULL) {
        int voltage_mv;
        adc_cali_raw_to_voltage(battery_cali_handle, raw, &voltage_mv);
        voltage = (voltage_mv / 1000.0f) * BATTERY_VOLTAGE_DIVIDER;
    } else {
        // Fallback: simple linear conversion
        // ADC with 12-bit resolution and 12dB attenuation has ~0-3.3V range
        voltage = (raw / 4095.0f) * 3.3f * BATTERY_VOLTAGE_DIVIDER;
    }

    return voltage;
}

// Get battery percentage (0-100)
static int battery_get_percentage(void) {
    float voltage = battery_read_voltage();

    if (voltage >= BATTERY_VOLTAGE_FULL) {
        return 100;
    }
    if (voltage <= BATTERY_VOLTAGE_EMPTY) {
        return 0;
    }

    // Linear interpolation between empty and full
    float percentage = (voltage - BATTERY_VOLTAGE_EMPTY) /
                       (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100.0f;
    return (int)percentage;
}

// Check if battery is low (below 20%)
static bool battery_is_low(void) {
    return battery_get_percentage() < 20;
}

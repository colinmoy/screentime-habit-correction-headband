#include <stdio.h>
#include <string.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// ─── AP Credentials ───────────────────────────────────────────────────────────
#define AP_SSID     "Headband-AP"
#define AP_PASS     "headband123"

// ─── Pin Definitions ──────────────────────────────────────────────────────────
#define PIN_ANGLE_LED               GPIO_NUM_4
#define PIN_DISTANCE_LED            GPIO_NUM_5
#define PIN_BUZZER                  GPIO_NUM_6
#define PIN_VIBRATION_MOTOR         GPIO_NUM_48
#define I2C_SCL_PIN                 GPIO_NUM_13
#define I2C_SDA_PIN                 GPIO_NUM_12
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

#define VL53L0X_ADDRESS             0x29
#define ICM42670_ADDRESS            0x68
#define VL53L0X_XSHUT_PIN           GPIO_NUM_2
#define BUTTON_CALIBRATE            GPIO_NUM_10

// ─── PWM Configuration ────────────────────────────────────────────────────────
#define PWM_TIMER                   LEDC_TIMER_0
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_MOTOR_CHANNEL           LEDC_CHANNEL_0
#define PWM_DISTANCE_LED_CHANNEL    LEDC_CHANNEL_1
#define PWM_ANGLE_LED_CHANNEL       LEDC_CHANNEL_2
#define PWM_BUZZER_CHANNEL          LEDC_CHANNEL_3
#define PWM_RESOLUTION              LEDC_TIMER_13_BIT
#define PWM_FREQ_HZ                 1000
#define BUZZER_FREQ_HZ              500

// ─── State Machine Parameters ─────────────────────────────────────────────────
const uint32_t thresholdDelay  = 3000;
const float    maxOutputPercent = 50.0;

static const char *TAG = "HEADBAND";

// ─── VL53L0X stop variable ───────────────────────────────────────────────────
static uint8_t stop_var = 0;

// ─── IMU Calibration ──────────────────────────────────────────────────────────
static float imu_angle_offset = 0.0;

// ─── Session Data (independent for each sensor) ───────────────────────────────
// Distance session: time from last distance reset to when bad distance started
static volatile bool     newDistanceSession       = false;
static volatile uint32_t lastDistanceTime         = 0;
static volatile uint32_t lastDistanceResetTime    = 0; // reset after each alarm clears

// Posture session: time from last posture reset to when bad posture started
static volatile bool     newPostureSession        = false;
static volatile uint32_t lastPostureTime          = 0;
static volatile uint32_t lastPostureResetTime     = 0; // reset after each alarm clears

static httpd_handle_t    server                   = NULL;

// ═══════════════════════════════════════════════════════════════════════════════
// I2C HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

esp_err_t write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t read_reg16(uint8_t dev_addr, uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *val = (buf[0] << 8) | buf[1];
    }
    return ret;
}

esp_err_t read_reg_multi(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

// ═══════════════════════════════════════════════════════════════════════════════
// VL53L0X
// ═══════════════════════════════════════════════════════════════════════════════

esp_err_t vl53l0x_init() {
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(VL53L0X_XSHUT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    uint8_t val;
    if (read_reg(VL53L0X_ADDRESS, 0xC0, &val) != ESP_OK || val != 0xEE) {
        ESP_LOGE(TAG, "VL53L0X not found!");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "VL53L0X detected (Model ID: 0x%02X)", val);

    write_reg(VL53L0X_ADDRESS, 0x80, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x01);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x00);
    read_reg(VL53L0X_ADDRESS, 0x91, &stop_var);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x80, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x0A, 0x04);
    write_reg(VL53L0X_ADDRESS, 0x0B, 0x01);

    ESP_LOGI(TAG, "VL53L0X init complete");
    return ESP_OK;
}

uint16_t vl53l0x_read_distance() {
    write_reg(VL53L0X_ADDRESS, 0x80, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x01);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x91, stop_var);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x80, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x01);

    vTaskDelay(pdMS_TO_TICKS(30));

    uint8_t status = 0;
    int timeout = 0;
    while (timeout < 100) {
        if (read_reg(VL53L0X_ADDRESS, 0x13, &status) == ESP_OK) {
            if (status & 0x07) break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout++;
    }

    uint16_t distance = 0;
    read_reg16(VL53L0X_ADDRESS, 0x1E, &distance);
    write_reg(VL53L0X_ADDRESS, 0x0B, 0x01);

    if (distance == 8191 || distance == 0) return 1200;
    return distance;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BMI160 IMU
// ═══════════════════════════════════════════════════════════════════════════════

#define BMI160_REG_CHIP_ID   0x00
#define BMI160_REG_ACC_DATA  0x12
#define BMI160_REG_ACC_CONF  0x40
#define BMI160_REG_ACC_RANGE 0x41
#define BMI160_REG_CMD       0x7E
#define BMI160_REG_PMU_STATUS 0x03

esp_err_t icm42670_init() {
    uint8_t chip_id;
    if (read_reg(ICM42670_ADDRESS, BMI160_REG_CHIP_ID, &chip_id) != ESP_OK) {
        ESP_LOGE(TAG, "BMI160 not responding!");
        return ESP_FAIL;
    }
    if (chip_id != 0xD1) {
        ESP_LOGE(TAG, "BMI160 CHIP_ID mismatch: 0x%02X (expected 0xD1)", chip_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BMI160 detected (CHIP_ID: 0x%02X)", chip_id);

    write_reg(ICM42670_ADDRESS, BMI160_REG_CMD, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(15));
    write_reg(ICM42670_ADDRESS, BMI160_REG_CMD, 0x11);
    vTaskDelay(pdMS_TO_TICKS(5));
    write_reg(ICM42670_ADDRESS, BMI160_REG_ACC_CONF, 0x28);
    write_reg(ICM42670_ADDRESS, BMI160_REG_ACC_RANGE, 0x05);

    uint8_t pmu_status;
    read_reg(ICM42670_ADDRESS, BMI160_REG_PMU_STATUS, &pmu_status);
    uint8_t acc_mode = (pmu_status >> 4) & 0x03;
    if (acc_mode != 0x01) {
        ESP_LOGE(TAG, "BMI160 accel not in normal mode (PMU_STATUS=0x%02X)", pmu_status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BMI160 init complete (PMU_STATUS=0x%02X)", pmu_status);
    return ESP_OK;
}

float icm42670_read_pitch() {
    uint8_t data[6];
    if (read_reg_multi(ICM42670_ADDRESS, BMI160_REG_ACC_DATA, data, 6) != ESP_OK) return 0.0;

    int16_t accel_x = (data[1] << 8) | data[0];
    int16_t accel_y = (data[3] << 8) | data[2];
    int16_t accel_z = (data[5] << 8) | data[4];
    if (accel_x == 0 && accel_y == 0 && accel_z == 0) return 0.0;

    float ax    = accel_x / 8192.0;
    float ay    = accel_y / 8192.0;
    float az    = accel_z / 8192.0;
    float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;

    return pitch - imu_angle_offset;
}

void calibrate_imu_zero_angle() {
    ESP_LOGI(TAG, "Calibrating BMI160 zero angle...");
    float angle_sum   = 0.0;
    int   valid_samples = 0;

    for (int i = 0; i < 20; i++) {
        uint8_t data[6];
        read_reg_multi(ICM42670_ADDRESS, BMI160_REG_ACC_DATA, data, 6);
        int16_t accel_x = (data[1] << 8) | data[0];
        int16_t accel_y = (data[3] << 8) | data[2];
        int16_t accel_z = (data[5] << 8) | data[4];
        if (accel_x == 0 && accel_y == 0 && accel_z == 0) continue;
        float ax    = accel_x / 8192.0;
        float ay    = accel_y / 8192.0;
        float az    = accel_z / 8192.0;
        float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;
        angle_sum += pitch;
        valid_samples++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (valid_samples > 0) {
        imu_angle_offset = angle_sum / valid_samples;
        ESP_LOGI(TAG, "Zero angle set to: %.2f degrees", imu_angle_offset);
    } else {
        imu_angle_offset = 0.0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// BUZZER / MOTOR
// ═══════════════════════════════════════════════════════════════════════════════

void buzzer_on() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 8191 - 512);
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

void buzzer_off() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 8191);
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

void motor_set_duty_percent(float percent) {
    uint32_t duty = (uint32_t)((percent / 100.0) * 8191.0);
    ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
}

void motor_off() {
    ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
}

// ═══════════════════════════════════════════════════════════════════════════════
// RECALIBRATION
// ═══════════════════════════════════════════════════════════════════════════════

void recalibrate_sensors() {
    ESP_LOGI(TAG, "=== RECALIBRATION START ===");
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(VL53L0X_XSHUT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    if (vl53l0x_init() != ESP_OK) {
        ESP_LOGE(TAG, "ToF re-init failed!");
    }
    calibrate_imu_zero_angle();

    // reset both timers on recalibration
    uint32_t now = esp_timer_get_time() / 1000;
    lastDistanceResetTime = now;
    lastPostureResetTime  = now;

    ESP_LOGI(TAG, "=== RECALIBRATION COMPLETE ===");
}

// ═══════════════════════════════════════════════════════════════════════════════
// HTTP HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

esp_err_t ping_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// GET /distance — app polls for distance sessions independently
esp_err_t distance_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    char response[128];
    if (newDistanceSession) {
        snprintf(response, sizeof(response),
            "{\"newSession\":true,\"distanceTime\":%lu}",
            lastDistanceTime);
        newDistanceSession = false;
    } else {
        snprintf(response, sizeof(response),
            "{\"newSession\":false,\"distanceTime\":null}");
    }
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// GET /posture — app polls for posture sessions independently
esp_err_t posture_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    char response[128];
    if (newPostureSession) {
        snprintf(response, sizeof(response),
            "{\"newSession\":true,\"postureTime\":%lu}",
            lastPostureTime);
        newPostureSession = false;
    } else {
        snprintf(response, sizeof(response),
            "{\"newSession\":false,\"postureTime\":null}");
    }
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

void start_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t ping_uri = {
        .uri = "/ping", .method = HTTP_GET,
        .handler = ping_handler, .user_ctx = NULL
    };
    httpd_uri_t distance_uri = {
        .uri = "/distance", .method = HTTP_GET,
        .handler = distance_handler, .user_ctx = NULL
    };
    httpd_uri_t posture_uri = {
        .uri = "/posture", .method = HTTP_GET,
        .handler = posture_handler, .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ping_uri);
        httpd_register_uri_handler(server, &distance_uri);
        httpd_register_uri_handler(server, &posture_uri);
        ESP_LOGI(TAG, "HTTP server started");
        ESP_LOGI(TAG, "  /ping     - connection test");
        ESP_LOGI(TAG, "  /distance - distance sessions");
        ESP_LOGI(TAG, "  /posture  - posture sessions");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// WIFI ACCESS POINT
// ═══════════════════════════════════════════════════════════════════════════════

void wifi_init() {
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {};
    strncpy((char *)ap_config.ap.ssid,     AP_SSID, sizeof(ap_config.ap.ssid));
    strncpy((char *)ap_config.ap.password, AP_PASS,  sizeof(ap_config.ap.password));
    ap_config.ap.ssid_len       = strlen(AP_SSID);
    ap_config.ap.max_connection = 1;
    ap_config.ap.authmode       = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  WiFi AP Started");
    ESP_LOGI(TAG, "  SSID:     %s", AP_SSID);
    ESP_LOGI(TAG, "  Password: %s", AP_PASS);
    ESP_LOGI(TAG, "  IP:       192.168.4.1");
    ESP_LOGI(TAG, "==========================================");

    start_http_server();
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  ESP32-S3 Posture Headband");
    ESP_LOGI(TAG, "===========================================");

    gpio_set_direction(VL53L0X_XSHUT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_config_t btn_conf = {
        .pin_bit_mask  = (1ULL << BUTTON_CALIBRATE),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    wifi_init();

    ledc_timer_config_t timer = {
        .speed_mode = PWM_MODE, .duty_resolution = PWM_RESOLUTION,
        .timer_num  = PWM_TIMER, .freq_hz = PWM_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_timer_config_t buzzer_timer = {
        .speed_mode = PWM_MODE, .duty_resolution = PWM_RESOLUTION,
        .timer_num  = LEDC_TIMER_1, .freq_hz = BUZZER_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&buzzer_timer);

    ledc_channel_config_t motor_ch = {
        .gpio_num = PIN_VIBRATION_MOTOR, .speed_mode = PWM_MODE,
        .channel  = PWM_MOTOR_CHANNEL, .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&motor_ch);

    ledc_channel_config_t distance_led_ch = {
        .gpio_num = PIN_DISTANCE_LED, .speed_mode = PWM_MODE,
        .channel  = PWM_DISTANCE_LED_CHANNEL, .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&distance_led_ch);

    ledc_channel_config_t angle_led_ch = {
        .gpio_num = PIN_ANGLE_LED, .speed_mode = PWM_MODE,
        .channel  = PWM_ANGLE_LED_CHANNEL, .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&angle_led_ch);

    ledc_channel_config_t buzzer_ch = {
        .gpio_num = PIN_BUZZER, .speed_mode = PWM_MODE,
        .channel  = PWM_BUZZER_CHANNEL, .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1, .duty = 8191, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&buzzer_ch);

    i2c_config_t conf = {
        .mode          = I2C_MODE_MASTER,
        .sda_io_num    = I2C_SDA_PIN,
        .scl_io_num    = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master        = {.clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags     = 0
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "Hardware configured");
    vTaskDelay(pdMS_TO_TICKS(100));

    if (vl53l0x_init() != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed!");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (icm42670_init() != ESP_OK) {
        ESP_LOGE(TAG, "BMI160 init failed!");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    calibrate_imu_zero_angle();

    // initialize both reset timers to startup time
    uint32_t startupTime      = esp_timer_get_time() / 1000;
    lastDistanceResetTime     = startupTime;
    lastPostureResetTime      = startupTime;

    ESP_LOGI(TAG, "=== READY | Starting measurements ===");

    uint32_t distanceStartTime     = 0;
    uint32_t postureStartTime      = 0;
    bool     distanceWarningActive = false;
    bool     postureWarningActive  = false;
    bool     distanceAlarmActive   = false;
    bool     postureAlarmActive    = false;
    uint32_t lastPrintTime         = 0;
    bool     button_pressed        = false;

    while (1) {
        uint32_t currentMillis = esp_timer_get_time() / 1000;

        // ─── Calibration button ───────────────────────────────────────────────
        if (gpio_get_level(BUTTON_CALIBRATE) == 0) {
            if (!button_pressed) {
                button_pressed = true;
                recalibrate_sensors();
            }
        } else {
            button_pressed = false;
        }

        uint16_t distance_mm = vl53l0x_read_distance();
        float    pitch        = icm42670_read_pitch();

        if (currentMillis - lastPrintTime >= 1000) {
            printf("Distance: %d mm | Pitch: %.1f°\n", distance_mm, pitch);
            lastPrintTime = currentMillis;
        }

        bool distanceBad = (distance_mm < 300);
        bool postureBad  = (fabs(pitch) >= 15.0);

        // ─── Distance State Machine ───────────────────────────────────────────
        float dist_motor_percent = 0.0f;
        bool  dist_buzzer        = false;

        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime     = currentMillis;
                ESP_LOGW(TAG, "Too close: %d mm", distance_mm);
            }

            uint32_t elapsedTime = currentMillis - distanceStartTime;

            if (elapsedTime < thresholdDelay) {
                dist_motor_percent = (float)elapsedTime / (float)thresholdDelay * maxOutputPercent;
                ledc_set_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL);
            } else {
                if (!distanceAlarmActive) {
                    distanceAlarmActive = true;
                    ESP_LOGE(TAG, "DISTANCE ALARM!");

                    ledc_set_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL, 8191);
                    ledc_update_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL);

                    lastDistanceTime   = (distanceStartTime - lastDistanceResetTime) / 1000;
                    newDistanceSession = true;
                    ESP_LOGI(TAG, "Distance session: good for %lu seconds before alarm", lastDistanceTime);
                }
                dist_motor_percent = maxOutputPercent;
                dist_buzzer        = true;
            }
        } else {
            if (distanceAlarmActive) {
                // alarm just cleared — reset the timer from now
                lastDistanceResetTime = currentMillis;
                ESP_LOGI(TAG, "Distance reset timer restarted");
            }
            if (distanceWarningActive || distanceAlarmActive) {
                ESP_LOGI(TAG, "Good distance restored");
            }
            distanceWarningActive = false;
            distanceAlarmActive   = false;

            ledc_set_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL, 0);
            ledc_update_duty(PWM_MODE, PWM_DISTANCE_LED_CHANNEL);
        }

        // ─── Posture State Machine ────────────────────────────────────────────
        float post_motor_percent = 0.0f;
        bool  post_buzzer        = false;

        if (postureBad) {
            if (!postureWarningActive) {
                postureWarningActive = true;
                postureStartTime     = currentMillis;
                ESP_LOGW(TAG, "Bad posture: %.1f°", pitch);
            }

            uint32_t elapsedTime = currentMillis - postureStartTime;

            if (elapsedTime < thresholdDelay) {
                post_motor_percent = (float)elapsedTime / (float)thresholdDelay * maxOutputPercent;
                ledc_set_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL);
            } else {
                if (!postureAlarmActive) {
                    postureAlarmActive = true;
                    ESP_LOGE(TAG, "POSTURE ALARM!");

                    ledc_set_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL, 8191);
                    ledc_update_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL);

                    lastPostureTime   = (postureStartTime - lastPostureResetTime) / 1000;
                    newPostureSession = true;
                    ESP_LOGI(TAG, "Posture session: good for %lu seconds before alarm", lastPostureTime);
                }
                post_motor_percent = maxOutputPercent;
                post_buzzer        = true;
            }
        } else {
            if (postureAlarmActive) {
                // alarm just cleared — reset the timer from now
                lastPostureResetTime = currentMillis;
                ESP_LOGI(TAG, "Posture reset timer restarted");
            }
            if (postureWarningActive || postureAlarmActive) {
                ESP_LOGI(TAG, "Good posture restored");
            }
            postureWarningActive = false;
            postureAlarmActive   = false;

            ledc_set_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL, 0);
            ledc_update_duty(PWM_MODE, PWM_ANGLE_LED_CHANNEL);
        }

        // ─── Combine outputs ──────────────────────────────────────────────────
        float motor_percent = (dist_motor_percent > post_motor_percent) ? dist_motor_percent : post_motor_percent;
        motor_set_duty_percent(motor_percent);
        if (dist_buzzer || post_buzzer) buzzer_on(); else buzzer_off();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
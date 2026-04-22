#include <stdio.h>
#include <string.h>
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
#define PIN_BUZZER                  GPIO_NUM_5
#define PIN_WARNING_LED             GPIO_NUM_6
#define PIN_VIBRATION_MOTOR         GPIO_NUM_7
#define I2C_SCL_PIN                 GPIO_NUM_8
#define I2C_SDA_PIN                 GPIO_NUM_9
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

#define VL53L0X_ADDRESS             0x29
#define VL53L0X_XSHUT_PIN           GPIO_NUM_2

#define PWM_TIMER                   LEDC_TIMER_0
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_MOTOR_CHANNEL           LEDC_CHANNEL_0
#define PWM_LED_CHANNEL             LEDC_CHANNEL_1
#define PWM_BUZZER_CHANNEL          LEDC_CHANNEL_2
#define PWM_RESOLUTION              LEDC_TIMER_13_BIT
#define PWM_FREQ_HZ                 1000
#define BUZZER_FREQ_HZ              2000

const uint32_t thresholdDelay  = 3000;
const float    maxOutputPercent = 50.0;

static const char *TAG = "HEADBAND";

// ─── VL53L0X stop variable (read from NVM during init) ────────────────────────
static uint8_t stop_var = 0;

// ─── Shared session data ──────────────────────────────────────────────────────
static volatile bool     newSessionReady  = false;
static volatile uint32_t lastDistanceTime = 0;
static httpd_handle_t    server           = NULL;

// ─── I2C Helpers ─────────────────────────────────────────────────────────────

esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t read_reg(uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t read_reg16(uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *val = (buf[0] << 8) | buf[1];
    }
    return ret;
}

// ─── VL53L0X ─────────────────────────────────────────────────────────────────

esp_err_t vl53l0x_simple_init() {
    // Pull LOW to reset, then HIGH to activate
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(VL53L0X_XSHUT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    uint8_t val;
    if (read_reg(0xC0, &val) != ESP_OK || val != 0xEE) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "VL53L0X found (0x%02X)", val);

    // Read stop variable from NVM — required for measurement sequence
    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    read_reg(0x91, &stop_var);
    ESP_LOGI(TAG, "VL53L0X stop_var = 0x%02X", stop_var);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);

    // Configure interrupt for new sample ready and clear any pending
    write_reg(0x0A, 0x04);
    write_reg(0x0B, 0x01);

    ESP_LOGI(TAG, "VL53L0X init complete");
    return ESP_OK;
}

uint16_t vl53l0x_read_distance() {
    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0x91, stop_var);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);
    write_reg(0x00, 0x01);
    vTaskDelay(pdMS_TO_TICKS(50));
    // Wait for SYSRANGE_START bit 0 to clear — measurement has begun
    uint8_t sysrange = 0;
    for (int i = 0; i < 100; i++) {
        read_reg(0x00, &sysrange);
        if (!(sysrange & 0x01)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    // Wait for RESULT_INTERRUPT_STATUS bits [2:0] to go non-zero — result ready
    uint8_t status = 0;
    for (int i = 0; i < 200; i++) {
        read_reg(0x13, &status);
        if (status & 0x07) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uint16_t distance = 0;
    read_reg16(0x1E, &distance);
    write_reg(0x0B, 0x01);  // clear interrupt
    return distance;
}

// ─── Buzzer ───────────────────────────────────────────────────────────────────

void buzzer_on() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 4096);
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

void buzzer_off() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

// ─── HTTP Handlers ────────────────────────────────────────────────────────────

esp_err_t ping_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t session_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    char response[128];
    if (newSessionReady) {
        snprintf(response, sizeof(response),
            "{\"newSession\":true,\"distanceTime\":%lu,\"postureTime\":null}",
            lastDistanceTime);
        newSessionReady = false;
    } else {
        snprintf(response, sizeof(response),
            "{\"newSession\":false,\"distanceTime\":null,\"postureTime\":null}");
    }
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

void start_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t ping_uri = {
        .uri      = "/ping",
        .method   = HTTP_GET,
        .handler  = ping_handler,
        .user_ctx = NULL
    };

    httpd_uri_t session_uri = {
        .uri      = "/session",
        .method   = HTTP_GET,
        .handler  = session_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ping_uri);
        httpd_register_uri_handler(server, &session_uri);
        ESP_LOGI(TAG, "HTTP server started");
    }
}

// ─── Wi-Fi Access Point ───────────────────────────────────────────────────────

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
    ap_config.ap.max_connection = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;


    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  Hotspot started!");
    ESP_LOGI(TAG, "  SSID:     %s", AP_SSID);
    ESP_LOGI(TAG, "  Password: %s", AP_PASS);
    ESP_LOGI(TAG, "  App IP:   192.168.4.1");
    ESP_LOGI(TAG, "==========================================");

    start_http_server();
}

// ─── Main ─────────────────────────────────────────────────────────────────────

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "  Posture Headband  ");

    // Hold sensor in reset (LOW) for 500ms to ensure full discharge from uncontrolled boot
    gpio_set_direction(VL53L0X_XSHUT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    wifi_init();

    // PWM Timer (motor and LED)
    ledc_timer_config_t timer = {
        .speed_mode      = PWM_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num       = PWM_TIMER,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    // Buzzer Timer
    ledc_timer_config_t buzzer_timer = {
        .speed_mode      = PWM_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = BUZZER_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&buzzer_timer);

    // Motor PWM channel
    ledc_channel_config_t motor_ch = {
        .gpio_num   = PIN_VIBRATION_MOTOR,
        .speed_mode = PWM_MODE,
        .channel    = PWM_MOTOR_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = {.output_invert = 0}
    };
    ledc_channel_config(&motor_ch);

    // LED PWM channel
    ledc_channel_config_t led_ch = {
        .gpio_num   = PIN_WARNING_LED,
        .speed_mode = PWM_MODE,
        .channel    = PWM_LED_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = {.output_invert = 0}
    };
    ledc_channel_config(&led_ch);

    // Buzzer PWM channel
    ledc_channel_config_t buzzer_ch = {
        .gpio_num   = PIN_BUZZER,
        .speed_mode = PWM_MODE,
        .channel    = PWM_BUZZER_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = {.output_invert = 0}
    };
    ledc_channel_config(&buzzer_ch);

    // I2C
    i2c_config_t conf = {
        .mode            = I2C_MODE_MASTER,
        .sda_io_num      = I2C_SDA_PIN,
        .scl_io_num      = I2C_SCL_PIN,
        .sda_pullup_en   = GPIO_PULLUP_ENABLE,
        .scl_pullup_en   = GPIO_PULLUP_ENABLE,
        .master          = {.clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags       = 0
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "Hardware configured");
    vTaskDelay(pdMS_TO_TICKS(100));

    if (vl53l0x_simple_init() != ESP_OK) {
        ESP_LOGE(TAG, "Sensor init failed!");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "  READY | Starting measurements...");

    uint32_t distanceStartTime    = 0;
    bool     distanceWarningActive = false;
    bool     distanceAlarmActive   = false;
    uint32_t lastPrintTime         = 0;
    uint32_t lastSensorReset       = 0;

    while (1) {
        uint32_t currentMillis = esp_timer_get_time() / 1000;

        // Only reset sensor when idle — never during an active warning or alarm
        if (!distanceWarningActive && !distanceAlarmActive &&
            currentMillis - lastSensorReset >= 5000) {
            if (vl53l0x_simple_init() != ESP_OK) {
                ESP_LOGE(TAG, "Sensor re-init failed");
            }
            lastSensorReset = currentMillis;
            continue;
        }

        uint16_t distance_mm = vl53l0x_read_distance();

        if (distance_mm > 2000 || distance_mm == 0) {
            distance_mm = 1200;
        }

        if (currentMillis - lastPrintTime >= 1000) {
            printf("Distance: %d mm (%.1f cm / %.2f inches)\n",
                   distance_mm, distance_mm / 10.0, distance_mm / 25.4);
            lastPrintTime = currentMillis;
        }

        bool distanceBad = (distance_mm < 300);

        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime     = currentMillis;
                ESP_LOGW(TAG, "Too close: %d mm", distance_mm);
            }

            uint32_t elapsedTime = currentMillis - distanceStartTime;

            if (elapsedTime < thresholdDelay) {
                float    fraction = (float)elapsedTime / (float)thresholdDelay;
                uint32_t duty     = (uint32_t)(((maxOutputPercent * fraction) / 100.0) * 8191.0);
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, duty);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, duty);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
            }
            else if (!distanceAlarmActive) {
                distanceAlarmActive = true;
                ESP_LOGE(TAG, "Alarm: Too close for 3+ seconds");

                uint32_t maxDuty = (uint32_t)((maxOutputPercent / 100.0) * 8191.0);
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, maxDuty);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, maxDuty);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                buzzer_on();

                // Record session: time from warning start to alarm in seconds
                lastDistanceTime = (currentMillis - distanceStartTime) / 1000;
                newSessionReady  = true;
                ESP_LOGI(TAG, "Session recorded: %lu seconds to alarm", lastDistanceTime);
            }
        } else {
            if (distanceWarningActive || distanceAlarmActive) {
                ESP_LOGI(TAG, "Good distance restored");
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                buzzer_off();
            }
            distanceWarningActive = false;
            distanceAlarmActive   = false;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
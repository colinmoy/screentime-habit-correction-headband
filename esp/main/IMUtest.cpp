#include <stdio.h>
#include <string.h>
#include <math.h>
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

// BMI160 — SA0/SDO to GND → address 0x68
#define BMI160_ADDRESS              0x68

// BMI160 registers
#define BMI160_REG_CHIP_ID          0x00  // read → 0xD1
#define BMI160_REG_PMU_STATUS       0x03  // bits[5:4]: acc mode (01=normal)
#define BMI160_REG_ACC_DATA         0x12  // ACC_X_LSB; 6 bytes, little-endian (X at 0x12, Y at 0x14, Z at 0x16)
#define BMI160_REG_ACC_CONF         0x40  // ODR + bandwidth
#define BMI160_REG_ACC_RANGE        0x41  // full-scale range
#define BMI160_REG_CMD              0x7E  // command register

#define PWM_TIMER                   LEDC_TIMER_0
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_MOTOR_CHANNEL           LEDC_CHANNEL_0
#define PWM_LED_CHANNEL             LEDC_CHANNEL_1
#define PWM_BUZZER_CHANNEL          LEDC_CHANNEL_2
#define PWM_RESOLUTION              LEDC_TIMER_13_BIT
#define PWM_FREQ_HZ                 1000
#define BUZZER_FREQ_HZ              2000

const uint32_t thresholdDelay    = 3000;
const float    maxOutputPercent  = 50.0f;
// Alarm triggers when tilt FROM HORIZONTAL exceeds this value.
// 0° = sensor flat, 90° = sensor fully upright. Tune this after mounting.
const float    IMU_TILT_THRESHOLD = 15.0f;

static const char *TAG = "HEADBAND";

static uint8_t stop_var = 0;

static volatile bool     newSessionReady  = false;
static volatile uint32_t lastDistanceTime = 0;
static httpd_handle_t    server           = NULL;

// ─── I2C Helpers ─────────────────────────────────────────────────────────────

esp_err_t write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, addr, buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t read_reg16(uint8_t addr, uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) *val = (buf[0] << 8) | buf[1];
    return ret;
}

// ─── VL53L0X (disabled — functions kept for re-enabling later) ───────────────

esp_err_t vl53l0x_simple_init() {
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(VL53L0X_XSHUT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    uint8_t val;
    if (read_reg(VL53L0X_ADDRESS, 0xC0, &val) != ESP_OK || val != 0xEE) return ESP_FAIL;
    write_reg(VL53L0X_ADDRESS, 0x80, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x01);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x00);
    read_reg(VL53L0X_ADDRESS, 0x91, &stop_var);
    write_reg(VL53L0X_ADDRESS, 0x00, 0x01);
    write_reg(VL53L0X_ADDRESS, 0xFF, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x80, 0x00);
    write_reg(VL53L0X_ADDRESS, 0x0A, 0x04);
    write_reg(VL53L0X_ADDRESS, 0x0B, 0x01);
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
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t sysrange = 0;
    for (int i = 0; i < 100; i++) {
        read_reg(VL53L0X_ADDRESS, 0x00, &sysrange);
        if (!(sysrange & 0x01)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uint8_t status = 0;
    for (int i = 0; i < 200; i++) {
        read_reg(VL53L0X_ADDRESS, 0x13, &status);
        if (status & 0x07) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uint8_t range_status = 0;
    read_reg(VL53L0X_ADDRESS, 0x14, &range_status);
    uint16_t distance = 0;
    if ((range_status >> 3) == 11) read_reg16(VL53L0X_ADDRESS, 0x1E, &distance);
    write_reg(VL53L0X_ADDRESS, 0x0B, 0x01);
    return distance;
}

// ─── BMI160 ───────────────────────────────────────────────────────────────────

esp_err_t bmi160_init() {
    // Confirm the chip responds and has the right ID
    uint8_t chip_id = 0;
    if (read_reg(BMI160_ADDRESS, BMI160_REG_CHIP_ID, &chip_id) != ESP_OK) {
        ESP_LOGE(TAG, "BMI160: no I2C ACK — check wiring and SA0 pin");
        return ESP_FAIL;
    }
    if (chip_id != 0xD1) {
        ESP_LOGE(TAG, "BMI160: unexpected CHIP_ID 0x%02X (expect 0xD1)", chip_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BMI160 found (CHIP_ID=0xD1)");

    // Soft reset — returns all registers to default (accel enters suspend mode)
    write_reg(BMI160_ADDRESS, BMI160_REG_CMD, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(15));  // datasheet: 10 ms after reset

    // Bring accelerometer out of suspend → normal mode via the CMD register.
    // This is the critical step — unlike MPU-6050, the BMI160 has NO global
    // sleep bit; each sensor block must be enabled individually through CMD.
    write_reg(BMI160_ADDRESS, BMI160_REG_CMD, 0x11);
    vTaskDelay(pdMS_TO_TICKS(5));   // datasheet: 3.8 ms max startup time

    // ACC_CONF: ODR = 100 Hz (0x08), BWP = OSR4 (0x0 << 4)
    // OSR4 applies 4x hardware oversampling → effective bandwidth 25 Hz, lower noise floor
    write_reg(BMI160_ADDRESS, BMI160_REG_ACC_CONF, 0x08);

    // ACC_RANGE: ±2g → 16384 LSB/g
    write_reg(BMI160_ADDRESS, BMI160_REG_ACC_RANGE, 0x03);

    // Verify the accelerometer actually entered normal mode
    uint8_t pmu = 0;
    read_reg(BMI160_ADDRESS, BMI160_REG_PMU_STATUS, &pmu);
    uint8_t acc_mode = (pmu >> 4) & 0x03;  // bits [5:4]
    if (acc_mode != 0x01) {
        ESP_LOGE(TAG, "BMI160: accel not in normal mode after CMD (PMU_STATUS=0x%02X)", pmu);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BMI160 init complete (PMU_STATUS=0x%02X)", pmu);
    return ESP_OK;
}

// Diagnostic suite — run once after init, read results on serial.
void bmi160_run_tests() {
    ESP_LOGI(TAG, "========== BMI160 Tests ==========");

    // T1: CHIP_ID must be 0xD1
    uint8_t chip_id = 0;
    esp_err_t err = read_reg(BMI160_ADDRESS, BMI160_REG_CHIP_ID, &chip_id);
    ESP_LOGI(TAG, "[T1] CHIP_ID    : %s  val=0x%02X  (expect 0xD1)",
             (err == ESP_OK && chip_id == 0xD1) ? "PASS" : "FAIL", chip_id);

    // T2: PMU_STATUS bits[5:4] = 0x01 means accel is in normal mode
    //     0x00 = suspended → all accel output registers frozen at 0
    uint8_t pmu = 0;
    err = read_reg(BMI160_ADDRESS, BMI160_REG_PMU_STATUS, &pmu);
    uint8_t acc_mode = (pmu >> 4) & 0x03;
    ESP_LOGI(TAG, "[T2] PMU_STATUS : %s  val=0x%02X  acc_mode=%d (expect 1=normal)",
             (err == ESP_OK && acc_mode == 1) ? "PASS" : "FAIL", pmu, acc_mode);
    if (acc_mode == 0)
        ESP_LOGE(TAG, "     !! ACCEL SUSPENDED — outputs will all be 0");

    // T3: ACC_CONF and ACC_RANGE
    uint8_t acconf = 0;
    err = read_reg(BMI160_ADDRESS, BMI160_REG_ACC_CONF, &acconf);
    ESP_LOGI(TAG, "[T3a] ACC_CONF  : %s  val=0x%02X  (expect 0x08: OSR4, 100Hz)",
             (err == ESP_OK && acconf == 0x08) ? "PASS" : "FAIL", acconf);
    uint8_t range = 0;
    err = read_reg(BMI160_ADDRESS, BMI160_REG_ACC_RANGE, &range);
    ESP_LOGI(TAG, "[T3b] ACC_RANGE : %s  val=0x%02X  (expect 0x03 for ±2g)",
             (err == ESP_OK && range == 0x03) ? "PASS" : "FAIL", range);

    // T4: Five raw accel samples
    //     Flat on a table: az ≈ ±16384 (1g), ax/ay ≈ 0
    //     NOTE: BMI160 data registers are little-endian (LSB byte first),
    //     opposite of MPU-6050.
    ESP_LOGI(TAG, "[T4] Raw accel samples (5x, 50 ms apart):");
    bool any_nonzero = false;
    for (int i = 0; i < 5; i++) {
        uint8_t buf[6] = {0};
        uint8_t reg = BMI160_REG_ACC_DATA;
        err = i2c_master_write_read_device(I2C_MASTER_NUM, BMI160_ADDRESS,
                                           &reg, 1, buf, 6, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "     [%d] I2C read failed (err=0x%X)", i, (unsigned)err);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        // Little-endian: LSB is buf[0], MSB is buf[1]
        int16_t ax = (int16_t)((buf[1] << 8) | buf[0]);
        int16_t ay = (int16_t)((buf[3] << 8) | buf[2]);
        int16_t az = (int16_t)((buf[5] << 8) | buf[4]);
        float gx = ax / 16384.0f, gy = ay / 16384.0f, gz = az / 16384.0f;
        float tilt = atan2f(sqrtf(gx * gx + gy * gy), fabsf(gz)) * (180.0f / (float)M_PI);
        ESP_LOGI(TAG, "     [%d] raw(%6d,%6d,%6d)  g(%.2f,%.2f,%.2f)  tilt=%.1f deg",
                 i, ax, ay, az, gx, gy, gz, tilt);
        if (ax != 0 || ay != 0 || az != 0) any_nonzero = true;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "[T4] %s", any_nonzero
             ? "PASS — non-zero accel data"
             : "FAIL — all zeros (accel still suspended or wrong address)");

    ESP_LOGI(TAG, "====================================");
}

// Gravity estimate — exponential low-pass filter state.
// Tracks the true gravity direction across calls, rejecting high-frequency noise.
static float g_x = 0.0f, g_y = 0.0f, g_z = 1.0f;
static bool  g_initialized = false;

// Returns tilt from horizontal in degrees (0° = flat, 90° = upright).
// Angle is computed against a filtered gravity direction, not a single raw sample,
// so vibration and ADC noise do not produce large angle errors.
// Returns -1.0 on I2C error or all-zero data (accel suspended).
float bmi160_read_tilt() {
    uint8_t buf[6];
    uint8_t reg = BMI160_REG_ACC_DATA;
    if (i2c_master_write_read_device(I2C_MASTER_NUM, BMI160_ADDRESS,
                                     &reg, 1, buf, 6, pdMS_TO_TICKS(100)) != ESP_OK) {
        return -1.0f;
    }
    // BMI160 is little-endian — LSB byte comes first
    int16_t ax_raw = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t ay_raw = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t az_raw = (int16_t)((buf[5] << 8) | buf[4]);

    if (ax_raw == 0 && ay_raw == 0 && az_raw == 0) return -1.0f;

    float ax = ax_raw / 16384.0f;
    float ay = ay_raw / 16384.0f;
    float az = az_raw / 16384.0f;

    // Seed the gravity estimate from the first valid reading
    if (!g_initialized) {
        g_x = ax;  g_y = ay;  g_z = az;
        g_initialized = true;
    }

    // Exponential low-pass filter — tracks the slow-moving gravity component.
    // alpha = 0.1 gives a ~2-second time constant at 200 ms loop rate, which
    // is slow enough to reject vibration/EMI and fast enough to follow real
    // head tilts over a few seconds.
    const float alpha = 0.1f;
    g_x = (1.0f - alpha) * g_x + alpha * ax;
    g_y = (1.0f - alpha) * g_y + alpha * ay;
    g_z = (1.0f - alpha) * g_z + alpha * az;

    // Normalize to a unit vector so magnitude drift does not affect the angle
    float mag = sqrtf(g_x * g_x + g_y * g_y + g_z * g_z);
    if (mag < 0.1f) return -1.0f;
    float nx = g_x / mag;
    float ny = g_y / mag;
    float nz = g_z / mag;

    // Angle between the sensor plane and horizontal (gravity direction).
    // 0° = sensor flat/horizontal, 90° = sensor fully upright.
    return atan2f(sqrtf(nx * nx + ny * ny), fabsf(nz)) * (180.0f / (float)M_PI);
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
    httpd_uri_t ping_uri    = { .uri="/ping",    .method=HTTP_GET, .handler=ping_handler,    .user_ctx=NULL };
    httpd_uri_t session_uri = { .uri="/session", .method=HTTP_GET, .handler=session_handler, .user_ctx=NULL };
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

    // Keep ToF XSHUT low so it doesn't float on the I2C bus while disabled
    gpio_set_direction(VL53L0X_XSHUT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VL53L0X_XSHUT_PIN, 0);

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
        .channel  = PWM_MOTOR_CHANNEL,   .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&motor_ch);

    ledc_channel_config_t led_ch = {
        .gpio_num = PIN_WARNING_LED,  .speed_mode = PWM_MODE,
        .channel  = PWM_LED_CHANNEL,  .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
    };
    ledc_channel_config(&led_ch);

    ledc_channel_config_t buzzer_ch = {
        .gpio_num = PIN_BUZZER,          .speed_mode = PWM_MODE,
        .channel  = PWM_BUZZER_CHANNEL,  .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1, .duty = 0, .hpoint = 0, .flags = {.output_invert = 0}
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

    // ToF disabled — call vl53l0x_simple_init() here to re-enable

    if (bmi160_init() != ESP_OK) {
        ESP_LOGE(TAG, "BMI160 init failed — halting");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    bmi160_run_tests();

    ESP_LOGI(TAG, "  READY | Starting measurements...");

    uint32_t imuStartTime     = 0;
    bool     imuWarningActive = false;
    bool     imuAlarmActive   = false;
    uint32_t lastPrintTime    = 0;

    while (1) {
        uint32_t currentMillis = esp_timer_get_time() / 1000;

        float tilt_deg = bmi160_read_tilt();

        // tilt_deg < 0 → I2C error or accel suspended; skip to avoid false alarms
        if (tilt_deg >= 0.0f && tilt_deg > IMU_TILT_THRESHOLD) {
            if (!imuWarningActive) {
                imuWarningActive = true;
                imuStartTime     = currentMillis;
                ESP_LOGW(TAG, "Bad posture: %.1f deg from horizontal", tilt_deg);
            }
            uint32_t elapsed = currentMillis - imuStartTime;
            uint32_t duty;
            if (elapsed < thresholdDelay) {
                float fraction = (float)elapsed / (float)thresholdDelay;
                duty = (uint32_t)(((maxOutputPercent * fraction) / 100.0f) * 8191.0f);
            } else {
                if (!imuAlarmActive) {
                    imuAlarmActive = true;
                    ESP_LOGE(TAG, "Alarm: bad posture for 3+ seconds");
                }
                duty = (uint32_t)((maxOutputPercent / 100.0f) * 8191.0f);
                buzzer_on();
            }
            ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, duty);
            ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
            ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, duty);
            ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
        } else if (tilt_deg >= 0.0f) {
            if (imuWarningActive || imuAlarmActive) {
                ESP_LOGI(TAG, "Good posture restored (%.1f deg)", tilt_deg);
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                buzzer_off();
            }
            imuWarningActive = false;
            imuAlarmActive   = false;
        }

        if (currentMillis - lastPrintTime >= 1000) {
            if (tilt_deg < 0.0f)
                printf("Tilt: ERROR (I2C failure or accel suspended)\n");
            else
                printf("Tilt: %.1f deg\n", tilt_deg);
            lastPrintTime = currentMillis;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

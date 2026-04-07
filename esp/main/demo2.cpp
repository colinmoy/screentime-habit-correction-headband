#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_log.h"

#define PIN_BUZZER                  GPIO_NUM_5
#define PIN_WARNING_LED             GPIO_NUM_6
#define PIN_VIBRATION_MOTOR         GPIO_NUM_7
#define I2C_SCL_PIN                 GPIO_NUM_8
#define I2C_SDA_PIN                 GPIO_NUM_9
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

#define VL53L0X_ADDRESS             0x29  // Moved to top

#define PWM_TIMER                   LEDC_TIMER_0
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_MOTOR_CHANNEL           LEDC_CHANNEL_0
#define PWM_LED_CHANNEL             LEDC_CHANNEL_1
#define PWM_BUZZER_CHANNEL          LEDC_CHANNEL_2
#define PWM_RESOLUTION              LEDC_TIMER_13_BIT
#define PWM_FREQ_HZ                 1000
#define BUZZER_FREQ_HZ              2000  // 2kHz square wave for buzzer

const uint32_t thresholdDelay = 3000;
const float maxOutputPercent = 50.0;

static const char *TAG = "HEADBAND";

// Basic I2C write
esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, buf, 2, pdMS_TO_TICKS(100));
}

// Basic I2C read
esp_err_t read_reg(uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

// Read 16-bit register
esp_err_t read_reg16(uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0X_ADDRESS, &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *val = (buf[0] << 8) | buf[1];
    }
    return ret;
}

// Absolute minimum init
esp_err_t vl53l0x_simple_init() {
    uint8_t val;
    
    // Check sensor
    if (read_reg(0xC0, &val) != ESP_OK || val != 0xEE) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "VL53L0X found (0x%02X)", val);
    
    // Minimal init from datasheet
    write_reg(0x88, 0x00);
    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0xFF, 0x00);
    write_reg(0x09, 0x00);
    write_reg(0x10, 0x00);
    write_reg(0x11, 0x00);
    write_reg(0x24, 0x01);
    write_reg(0x25, 0xff);
    write_reg(0x75, 0x00);
    write_reg(0xFF, 0x01);
    write_reg(0x4e, 0x2c);
    write_reg(0x48, 0x00);
    write_reg(0x30, 0x20);
    write_reg(0xFF, 0x00);
    write_reg(0x30, 0x09);
    write_reg(0x54, 0x00);
    write_reg(0x31, 0x04);
    write_reg(0x32, 0x03);
    write_reg(0x40, 0x83);
    write_reg(0x46, 0x25);
    write_reg(0x60, 0x00);
    write_reg(0x27, 0x00);
    write_reg(0x50, 0x06);
    write_reg(0x51, 0x00);
    write_reg(0x52, 0x96);
    write_reg(0x56, 0x08);
    write_reg(0x57, 0x30);
    write_reg(0x61, 0x00);
    write_reg(0x62, 0x00);
    write_reg(0x64, 0x00);
    write_reg(0x65, 0x00);
    write_reg(0x66, 0xa0);
    write_reg(0xFF, 0x01);
    write_reg(0x22, 0x32);
    write_reg(0x47, 0x14);
    write_reg(0x49, 0xff);
    write_reg(0x4a, 0x00);
    write_reg(0xFF, 0x00);
    write_reg(0x7a, 0x0a);
    write_reg(0x7b, 0x00);
    write_reg(0x78, 0x21);
    write_reg(0xFF, 0x01);
    write_reg(0x23, 0x34);
    write_reg(0x42, 0x00);
    write_reg(0x44, 0xff);
    write_reg(0x45, 0x26);
    write_reg(0x46, 0x05);
    write_reg(0x40, 0x40);
    write_reg(0x0e, 0x06);
    write_reg(0x20, 0x1a);
    write_reg(0x43, 0x40);
    write_reg(0xFF, 0x00);
    write_reg(0x34, 0x03);
    write_reg(0x35, 0x44);
    write_reg(0xFF, 0x01);
    write_reg(0x31, 0x04);
    write_reg(0x4b, 0x09);
    write_reg(0x4c, 0x05);
    write_reg(0x4d, 0x04);
    write_reg(0xFF, 0x00);
    write_reg(0x44, 0x00);
    write_reg(0x45, 0x20);
    write_reg(0x47, 0x08);
    write_reg(0x48, 0x28);
    write_reg(0x67, 0x00);
    write_reg(0x70, 0x04);
    write_reg(0x71, 0x01);
    write_reg(0x72, 0xfe);
    write_reg(0x76, 0x00);
    write_reg(0x77, 0x00);
    write_reg(0xFF, 0x01);
    write_reg(0x0d, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x01);
    write_reg(0x01, 0xf8);
    write_reg(0xFF, 0x01);
    write_reg(0x8e, 0x01);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Let it settle
    
    ESP_LOGI(TAG, "Init complete");
    return ESP_OK;
}

// Single-shot measurement
uint16_t vl53l0x_read_distance() {
    // Start single measurement
    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0x91, 0x3C);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);
    write_reg(0x00, 0x01);
    
    // Wait for measurement
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Check if ready
    uint8_t status;
    for (int i = 0; i < 100; i++) {
        read_reg(0x13, &status);
        if (status & 0x07) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Read distance
    uint16_t distance;
    read_reg16(0x1E, &distance);
    
    // Clear interrupt
    write_reg(0x0B, 0x01);
    
    return distance;
}

// Turn buzzer ON (50% duty cycle square wave at 2kHz)
void buzzer_on() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 4096);  // 50% duty cycle
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

// Turn buzzer OFF
void buzzer_off() {
    ledc_set_duty(PWM_MODE, PWM_BUZZER_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_BUZZER_CHANNEL);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===================================================");
    ESP_LOGI(TAG, "  Posture Headband - Simple Mode");
    ESP_LOGI(TAG, "===================================================");
    
    // PWM Timer (for motor and LED)
    ledc_timer_config_t timer = {
        .speed_mode = PWM_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);
    
    // Buzzer Timer (separate timer for different frequency)
    ledc_timer_config_t buzzer_timer = {
        .speed_mode = PWM_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&buzzer_timer);
    
    // Motor PWM channel
    ledc_channel_config_t motor = {
        .gpio_num = PIN_VIBRATION_MOTOR,
        .speed_mode = PWM_MODE,
        .channel = PWM_MOTOR_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = {.output_invert = 0}
    };
    ledc_channel_config(&motor);
    
    // LED PWM channel
    ledc_channel_config_t led = {
        .gpio_num = PIN_WARNING_LED,
        .speed_mode = PWM_MODE,
        .channel = PWM_LED_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = {.output_invert = 0}
    };
    ledc_channel_config(&led);
    
    // Buzzer PWM channel (square wave)
    ledc_channel_config_t buzzer = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = PWM_MODE,
        .channel = PWM_BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
        .flags = {.output_invert = 0}
    };
    ledc_channel_config(&buzzer);
    
    // I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags = 0
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    
    ESP_LOGI(TAG, "Hardware configured");
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (vl53l0x_simple_init() != ESP_OK) {
        ESP_LOGE(TAG, "Init failed!");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "===================================================");
    ESP_LOGI(TAG, "  READY! Starting measurements...");
    ESP_LOGI(TAG, "===================================================");
    
    uint32_t distanceStartTime = 0;
    bool distanceWarningActive = false;
    bool distanceAlarmActive = false;
    uint32_t lastPrintTime = 0;
    
    while (1) {
        uint16_t distance_mm = vl53l0x_read_distance();
        
        // Filter out-of-range
        if (distance_mm > 2000 || distance_mm == 0) {
            distance_mm = 1200;
        }
        
        uint32_t currentTime = esp_timer_get_time() / 1000;
        if (currentTime - lastPrintTime >= 1000) {
            printf("Distance: %d mm (%.1f cm / %.2f inches)\n", 
                   distance_mm, distance_mm / 10.0, distance_mm / 25.4);
            lastPrintTime = currentTime;
        }
        
        bool distanceBad = (distance_mm < 300);
        uint32_t currentMillis = esp_timer_get_time() / 1000;
        
        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime = currentMillis;
                ESP_LOGW(TAG, "Too close! %d mm", distance_mm);
            }
            
            uint32_t elapsedTime = currentMillis - distanceStartTime;
            
            if (elapsedTime < thresholdDelay) {
                // Ramp up motor and LED
                float fraction = (float)elapsedTime / (float)thresholdDelay;
                uint32_t duty = (uint32_t)(((maxOutputPercent * fraction) / 100.0) * 8191.0);
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, duty);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, duty);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
            }
            else if (!distanceAlarmActive) {
                distanceAlarmActive = true;
                ESP_LOGE(TAG, "ALARM! Too close for 3+ seconds");
                
                // Full motor and LED
                uint32_t maxDuty = (uint32_t)((maxOutputPercent / 100.0) * 8191.0);
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, maxDuty);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, maxDuty);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                
                // Buzzer ON (2kHz square wave)
                buzzer_on();
            }
        } else {
            if (distanceWarningActive || distanceAlarmActive) {
                ESP_LOGI(TAG, "Good distance restored");
                
                // Turn off all feedback
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                buzzer_off();
            }
            distanceWarningActive = false;
            distanceAlarmActive = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
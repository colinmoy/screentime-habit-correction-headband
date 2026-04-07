#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h" 
#include "esp_timer.h"

// Hardware Pins
#define PIN_VIBRATION_MOTOR         GPIO_NUM_21
#define PIN_BUZZER                  GPIO_NUM_22
#define PIN_WARNING_LED             GPIO_NUM_23 

// PWM Configurations
#define PWM_TIMER                   LEDC_TIMER_0
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_MOTOR_CHANNEL           LEDC_CHANNEL_0
#define PWM_LED_CHANNEL             LEDC_CHANNEL_1 
#define PWM_RESOLUTION              LEDC_TIMER_13_BIT 
#define PWM_FREQ_HZ                 1000              

// Feedback Variables
const uint32_t thresholdDelay = 3000; 
const float maxOutputPercent = 50.0; 

void app_main(void)
{
    // Initialize discrete buzzer output
    gpio_config_t io_conf_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_out);

    // Initialize shared PWM timer for proportional feedback
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RESOLUTION,
        .freq_hz          = PWM_FREQ_HZ,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure PWM channels for motor and LED
    ledc_channel_config_t motor_channel = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_MOTOR_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_VIBRATION_MOTOR,
        .duty           = 0, 
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&motor_channel));

    ledc_channel_config_t led_channel = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_LED_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_WARNING_LED,
        .duty           = 0, 
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&led_channel));

    printf("System Ready. Monitoring screen distance...\n");

    uint32_t distanceStartTime = 0;
    bool distanceWarningActive = false;
    bool distanceAlarmActive = false;

    while (1) {
        // TODO: Replace with actual I2C ToF sensor read
        uint16_t currentDistanceMm = 200; 
        bool distanceBad = (currentDistanceMm < 300);
        uint32_t currentMillis = esp_timer_get_time() / 1000;

        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime = currentMillis;
                printf("[WARNING] Threshold crossed. Initiating proportional feedback.\n");
            }
            
            uint32_t elapsedTime = currentMillis - distanceStartTime;

            // Apply proportional PWM feedback during the warning window
            if (elapsedTime < thresholdDelay) {
                float timeFraction = (float)elapsedTime / (float)thresholdDelay;
                uint32_t dutyCycle = (uint32_t)(((maxOutputPercent * timeFraction) / 100.0) * 8191.0);
                
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, dutyCycle);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, dutyCycle);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
            } 
            // Activate full alarm state once threshold time is met
            else if (!distanceAlarmActive) {
                distanceAlarmActive = true;
                
                uint32_t maxDutyCycle = (uint32_t)((maxOutputPercent / 100.0) * 8191.0);
                
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, maxDutyCycle);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, maxDutyCycle);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                
                gpio_set_level(PIN_BUZZER, 1);
                printf("[ALARM] Threshold time met. Activating buzzer.\n");
            }
        } else {
            // Clear all states and deactivate outputs when posture is corrected
            if (distanceWarningActive || distanceAlarmActive) {
                printf("[IDLE] Distance corrected. Resetting outputs.\n");
                
                ledc_set_duty(PWM_MODE, PWM_MOTOR_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_MOTOR_CHANNEL);
                
                ledc_set_duty(PWM_MODE, PWM_LED_CHANNEL, 0);
                ledc_update_duty(PWM_MODE, PWM_LED_CHANNEL);
                
                gpio_set_level(PIN_BUZZER, 0);
            }
            distanceWarningActive = false;
            distanceAlarmActive = false;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
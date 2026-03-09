#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// ESP32-C6 Pin Definitions
#define BUTTON_POSTURE   GPIO_NUM_4
#define BUTTON_DISTANCE  GPIO_NUM_5
#define LED_WARNING      GPIO_NUM_21
#define LED_POSTURE_ALARM GPIO_NUM_22
#define LED_DISTANCE_ALARM GPIO_NUM_23

void app_main(void)
{
    // 1. Configure Input Buttons (with internal pull-ups)
    gpio_config_t io_conf_in = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_POSTURE) | (1ULL << BUTTON_DISTANCE),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf_in);

    // 2. Configure Output LEDs
    gpio_config_t io_conf_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_WARNING) | (1ULL << LED_POSTURE_ALARM) | (1ULL << LED_DISTANCE_ALARM),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_out);

    printf("System Ready. Monitoring posture and screen distance...\n");

    // Timing and State Variables
    uint32_t postureStartTime = 0;
    uint32_t distanceStartTime = 0;
    const uint32_t thresholdDelay = 3000; // 3.0 seconds

    bool postureWarningActive = false;
    bool distanceWarningActive = false;
    bool postureAlarmActive = false;
    bool distanceAlarmActive = false;

    // Equivalent to Arduino's loop()
    while (1) {
        // Read simulated sensor states (0 means pressed due to pull-up)
        bool postureBad = (gpio_get_level(BUTTON_POSTURE) == 0);
        bool distanceBad = (gpio_get_level(BUTTON_DISTANCE) == 0);

        // Get current time in milliseconds
        uint32_t currentMillis = esp_timer_get_time() / 1000;

        // --- Posture Logic (IMU Simulation) ---
        if (postureBad) {
            if (!postureWarningActive) {
                postureWarningActive = true;
                postureStartTime = currentMillis;
                printf("[WARNING] Bad posture detected. Activating moderate feedback.\n");
            }
            
            if (!postureAlarmActive && (currentMillis - postureStartTime >= thresholdDelay)) {
                postureAlarmActive = true;
                printf("[ALARM] Posture threshold exceeded (3s). Activating full feedback.\n");
            }
        } else {
            if (postureWarningActive || postureAlarmActive) {
                printf("[IDLE] Posture corrected.\n");
            }
            postureWarningActive = false;
            postureAlarmActive = false;
        }

        // --- Distance Logic (ToF Simulation) ---
        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime = currentMillis;
                printf("[WARNING] Screen too close. Activating moderate feedback.\n");
            }
            
            if (!distanceAlarmActive && (currentMillis - distanceStartTime >= thresholdDelay)) {
                distanceAlarmActive = true;
                printf("[ALARM] Distance threshold exceeded (3s). Activating full feedback.\n");
            }
        } else {
            if (distanceWarningActive || distanceAlarmActive) {
                printf("[IDLE] Distance corrected.\n");
            }
            distanceWarningActive = false;
            distanceAlarmActive = false;
        }

        // --- Hardware Output Control ---
        gpio_set_level(LED_WARNING, (postureWarningActive || distanceWarningActive) ? 1 : 0);
        gpio_set_level(LED_POSTURE_ALARM, postureAlarmActive ? 1 : 0);
        gpio_set_level(LED_DISTANCE_ALARM, distanceAlarmActive ? 1 : 0);

        // Yield to the FreeRTOS scheduler to prevent watchdog timer resets
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
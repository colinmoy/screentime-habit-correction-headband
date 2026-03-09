#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BUTTON_POSTURE     GPIO_NUM_4
#define BUTTON_DISTANCE    GPIO_NUM_5
#define LED_POSTURE_WARN   GPIO_NUM_21
#define LED_DISTANCE_WARN  GPIO_NUM_22
#define LED_GLOBAL_ALARM   GPIO_NUM_23

void app_main(void)
{
    gpio_config_t io_conf_in = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_POSTURE) | (1ULL << BUTTON_DISTANCE),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf_in);

    gpio_config_t io_conf_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_POSTURE_WARN) | (1ULL << LED_DISTANCE_WARN) | (1ULL << LED_GLOBAL_ALARM),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_out);

    printf("System Ready. Monitoring posture and screen distance...\n");

    uint32_t postureStartTime = 0;
    uint32_t distanceStartTime = 0;
    const uint32_t thresholdDelay = 3000; 

    bool postureWarningActive = false;
    bool distanceWarningActive = false;
    bool postureAlarmActive = false;
    bool distanceAlarmActive = false;

    while (1) {
        bool postureBad = (gpio_get_level(BUTTON_POSTURE) == 0);
        bool distanceBad = (gpio_get_level(BUTTON_DISTANCE) == 0);

        uint32_t currentMillis = esp_timer_get_time() / 1000;

        if (postureBad) {
            if (!postureWarningActive) {
                postureWarningActive = true;
                postureStartTime = currentMillis;
                printf("[WARNING] Bad posture detected. Activating posture LED.\n");
            }
            
            if (!postureAlarmActive && (currentMillis - postureStartTime >= thresholdDelay)) {
                postureAlarmActive = true;
                printf("[ALARM] Posture threshold exceeded (3s). Activating global alarm LED.\n");
            }
        } else {
            if (postureWarningActive || postureAlarmActive) {
                printf("[IDLE] Posture corrected.\n");
            }
            postureWarningActive = false;
            postureAlarmActive = false;
        }

        if (distanceBad) {
            if (!distanceWarningActive) {
                distanceWarningActive = true;
                distanceStartTime = currentMillis;
                printf("[WARNING] Screen too close. Activating distance LED.\n");
            }
            
            if (!distanceAlarmActive && (currentMillis - distanceStartTime >= thresholdDelay)) {
                distanceAlarmActive = true;
                printf("[ALARM] Distance threshold exceeded (3s). Activating global alarm LED.\n");
            }
        } else {
            if (distanceWarningActive || distanceAlarmActive) {
                printf("[IDLE] Distance corrected.\n");
            }
            distanceWarningActive = false;
            distanceAlarmActive = false;
        }

        gpio_set_level(LED_POSTURE_WARN, postureWarningActive ? 1 : 0);
        gpio_set_level(LED_DISTANCE_WARN, distanceWarningActive ? 1 : 0);
        
        gpio_set_level(LED_GLOBAL_ALARM, (postureAlarmActive || distanceAlarmActive) ? 1 : 0);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
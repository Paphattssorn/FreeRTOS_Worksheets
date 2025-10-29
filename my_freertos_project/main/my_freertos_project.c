#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// เปลี่ยนมาใช้ LED บน DevKit (อาจแตกต่างตามรุ่น)
#define LED1_PIN GPIO_NUM_2  // Built-in LED บน DevKit
#define LED2_PIN GPIO_NUM_4  // External LED
#define BUTTON_PIN GPIO_NUM_0

static const char *TAG = "LED_TEST";

void app_main(void)
{
    // GPIO Configuration
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED1_PIN) | (1ULL << LED2_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Reset LED states
    gpio_set_level(LED1_PIN, 0);
    gpio_set_level(LED2_PIN, 0);

    // Button configuration
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << BUTTON_PIN;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Single Task System Started");

    // Simple LED test loop
    while (1) {
        // Test LED1 (Built-in LED)
        ESP_LOGI(TAG, "Testing LED1 (GPIO %d)", LED1_PIN);
        gpio_set_level(LED1_PIN, 1);  // Turn ON
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED1_PIN, 0);  // Turn OFF
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Test LED2
        ESP_LOGI(TAG, "Testing LED2 (GPIO %d)", LED2_PIN);
        gpio_set_level(LED2_PIN, 1);  // Turn ON
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED2_PIN, 0);  // Turn OFF
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
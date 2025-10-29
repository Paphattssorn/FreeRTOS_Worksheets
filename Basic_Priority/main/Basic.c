#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LED_HIGH_PIN GPIO_NUM_2    // สำหรับ High Priority Task
#define LED_MED_PIN  GPIO_NUM_4    // สำหรับ Medium Priority Task
#define LED_LOW_PIN  GPIO_NUM_5    // สำหรับ Low Priority Task
#define BUTTON_PIN   GPIO_NUM_0    // Trigger button

static const char *TAG = "PRIORITY_DEMO";

// Global variables for statistics
volatile uint32_t high_task_count = 0;
volatile uint32_t med_task_count = 0;
volatile uint32_t low_task_count = 0;
volatile bool priority_test_running = false;

// สำหรับ Priority Inversion Demo
volatile bool shared_resource_busy = false;

// ---------------------- HIGH PRIORITY TASK ----------------------
void high_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "High Priority Task started (Priority 5)");
    
    while (1) {
        if (priority_test_running) {
            high_task_count++;
            ESP_LOGI(TAG, "HIGH PRIORITY RUNNING (%d)", high_task_count);
            
            gpio_set_level(LED_HIGH_PIN, 1);
            for (int i = 0; i < 100000; i++) {
                volatile int dummy = i * 2;
            }
            gpio_set_level(LED_HIGH_PIN, 0);
            
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ---------------------- MEDIUM PRIORITY TASK ----------------------
void medium_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Medium Priority Task started (Priority 3)");
    
    while (1) {
        if (priority_test_running) {
            med_task_count++;
            ESP_LOGI(TAG, "Medium priority running (%d)", med_task_count);
            
            gpio_set_level(LED_MED_PIN, 1);
            for (int i = 0; i < 200000; i++) {
                volatile int dummy = i + 100;
            }
            gpio_set_level(LED_MED_PIN, 0);
            
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ---------------------- LOW PRIORITY TASK ----------------------
void low_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Low Priority Task started (Priority 1)");
    
    while (1) {
        if (priority_test_running) {
            low_task_count++;
            ESP_LOGI(TAG, "Low priority running (%d)", low_task_count);
            
            gpio_set_level(LED_LOW_PIN, 1);
            for (int i = 0; i < 500000; i++) {
                volatile int dummy = i - 50;
                if (i % 100000 == 0) {
                    vTaskDelay(1); // ป้องกัน watchdog
                }
            }
            gpio_set_level(LED_LOW_PIN, 0);
            
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ---------------------- EQUAL PRIORITY TASKS ----------------------
void equal_priority_task(void *pvParameters)
{
    int task_id = (int)pvParameters;

    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", task_id);
            for (int i = 0; i < 300000; i++) {
                volatile int dummy = i;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------------- PRIORITY INVERSION DEMO ----------------------
void priority_inversion_high(void *pvParameters)
{
    ESP_LOGI(TAG, "Priority Inversion HIGH started (Priority 5)");
    while (1) {
        if (priority_test_running) {
            ESP_LOGW(TAG, "High priority task needs shared resource");

            // รอ shared resource
            while (shared_resource_busy) {
                ESP_LOGW(TAG, "High priority BLOCKED by low priority!");
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            shared_resource_busy = true;
            ESP_LOGI(TAG, "High priority task got resource");

            // ใช้ resource ชั่วครู่
            vTaskDelay(pdMS_TO_TICKS(100));
            shared_resource_busy = false;
            ESP_LOGI(TAG, "High priority released resource");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void priority_inversion_low(void *pvParameters)
{
    ESP_LOGI(TAG, "Priority Inversion LOW started (Priority 1)");
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Low priority task using shared resource");
            shared_resource_busy = true;

            // จำลองการใช้ resource นาน
            vTaskDelay(pdMS_TO_TICKS(2000));

            shared_resource_busy = false;
            ESP_LOGI(TAG, "Low priority task released resource");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ---------------------- CONTROL TASK ----------------------
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");

    while (1) {
        // กดปุ่มเริ่มทดสอบ
        if (gpio_get_level(BUTTON_PIN) == 0) {
            if (!priority_test_running) {
                ESP_LOGW(TAG, "=== STARTING PRIORITY TEST ===");
                high_task_count = med_task_count = low_task_count = 0;
                priority_test_running = true;

                // รันทดสอบ 10 วินาที
                vTaskDelay(pdMS_TO_TICKS(10000));

                priority_test_running = false;

                ESP_LOGW(TAG, "=== PRIORITY TEST RESULTS ===");
                ESP_LOGI(TAG, "High Priority Task runs: %d", high_task_count);
                ESP_LOGI(TAG, "Medium Priority Task runs: %d", med_task_count);
                ESP_LOGI(TAG, "Low Priority Task runs: %d", low_task_count);

                uint32_t total_runs = high_task_count + med_task_count + low_task_count;
                if (total_runs > 0) {
                    ESP_LOGI(TAG, "High priority %%: %.1f%%",
                             (float)high_task_count / total_runs * 100);
                    ESP_LOGI(TAG, "Medium priority %%: %.1f%%",
                             (float)med_task_count / total_runs * 100);
                    ESP_LOGI(TAG, "Low priority %%: %.1f%%",
                             (float)low_task_count / total_runs * 100);
                }

                // หลังจบการทดสอบหลัก เริ่มสาธิต Priority Inversion
                ESP_LOGW(TAG, "=== STARTING PRIORITY INVERSION DEMO ===");
                vTaskDelay(pdMS_TO_TICKS(2000));
                priority_test_running = true;
                vTaskDelay(pdMS_TO_TICKS(8000));
                priority_test_running = false;
                ESP_LOGW(TAG, "=== PRIORITY INVERSION DEMO FINISHED ===");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------------------- MAIN APP ----------------------
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Priority Scheduling Demo ===");

    // ตั้งค่า GPIO สำหรับ LED
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_HIGH_PIN) | (1ULL << LED_MED_PIN) | (1ULL << LED_LOW_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // ตั้งค่าปุ่ม
    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "Creating tasks with different priorities...");

    // สร้าง tasks สำหรับ Priority Scheduling
    xTaskCreate(high_priority_task, "HighPrio", 3072, NULL, 5, NULL);
    xTaskCreate(medium_priority_task, "MedPrio", 3072, NULL, 3, NULL);
    xTaskCreate(low_priority_task, "LowPrio", 3072, NULL, 1, NULL);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, NULL);

    // สร้าง tasks ที่มี Priority เท่ากัน (2)
    xTaskCreate(equal_priority_task, "Equal1", 2048, (void *)1, 2, NULL);
    xTaskCreate(equal_priority_task, "Equal2", 2048, (void *)2, 2, NULL);
    xTaskCreate(equal_priority_task, "Equal3", 2048, (void *)3, 2, NULL);

    // สร้าง Priority Inversion tasks
    xTaskCreate(priority_inversion_high, "PI_High", 3072, NULL, 5, NULL);
    xTaskCreate(priority_inversion_low,  "PI_Low",  3072, NULL, 1, NULL);

    ESP_LOGI(TAG, "Press button to start priority test");
    ESP_LOGI(TAG, "Watch LEDs: GPIO2=High, GPIO4=Med, GPIO5=Low priority");
}

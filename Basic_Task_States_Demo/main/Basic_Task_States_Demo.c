#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING GPIO_NUM_2
#define LED_READY GPIO_NUM_4
#define LED_BLOCKED GPIO_NUM_5
#define LED_SUSPENDED GPIO_NUM_18

#define BUTTON1_PIN GPIO_NUM_0
#define BUTTON2_PIN GPIO_NUM_15

static const char *TAG = "TASK_STATES";

// Handles
TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;

// Semaphore
SemaphoreHandle_t demo_semaphore = NULL;

// Exercise 1: State transition counter
volatile uint32_t state_changes[5] = {0};

const char* state_names[] = {
    "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"
};

const char* get_state_name(eTaskState state)
{
    if (state <= eDeleted) return state_names[state];
    return state_names[5];
}

void count_state_change(eTaskState old_state, eTaskState new_state)
{
    if (old_state != new_state && new_state <= eDeleted) {
        state_changes[new_state]++;
        ESP_LOGI(TAG, "State change: %s -> %s (Count: %d)",
                 get_state_name(old_state),
                 get_state_name(new_state),
                 state_changes[new_state]);
    }
}

// ✅ Exercise 2: Custom State Indicator
void update_state_display(eTaskState current_state)
{
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);
    
    switch (current_state) {
        case eRunning:
            gpio_set_level(LED_RUNNING, 1);
            break;
        case eReady:
            gpio_set_level(LED_READY, 1);
            break;
        case eBlocked:
            gpio_set_level(LED_BLOCKED, 1);
            break;
        case eSuspended:
            gpio_set_level(LED_SUSPENDED, 1);
            break;
        default:
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED_RUNNING, 1);
                gpio_set_level(LED_READY, 1);
                gpio_set_level(LED_BLOCKED, 1);
                gpio_set_level(LED_SUSPENDED, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_RUNNING, 0);
                gpio_set_level(LED_READY, 0);
                gpio_set_level(LED_BLOCKED, 0);
                gpio_set_level(LED_SUSPENDED, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
    }
}

// -------------------- State Demo Task --------------------
void state_demo_task(void *pvParameters)
{
    ESP_LOGI(TAG, "State Demo Task started");
    eTaskState old_state = eReady;

    while (1) {
        // Running
        count_state_change(old_state, eRunning);
        update_state_display(eRunning);
        old_state = eRunning;
        ESP_LOGI(TAG, "Task is RUNNING");
        for (int i = 0; i < 1000000; i++) { volatile int d = i * 2; }

        // Ready
        count_state_change(old_state, eReady);
        update_state_display(eReady);
        old_state = eReady;
        ESP_LOGI(TAG, "Task will be READY");
        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(100));

        // Blocked
        count_state_change(old_state, eBlocked);
        update_state_display(eBlocked);
        old_state = eBlocked;
        ESP_LOGI(TAG, "Task waiting for semaphore");
        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore");
        } else {
            ESP_LOGI(TAG, "Timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -------------------- Control Task --------------------
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");
    bool suspended = false;
    int control_cycle = 0;
    static bool external_deleted = false;

    while (1) {
        control_cycle++;

        // Button 1 = Suspend / Resume
        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            eTaskState old_state = eTaskGetState(state_demo_task_handle);

            if (!suspended) {
                ESP_LOGW(TAG, "Suspending Task");
                vTaskSuspend(state_demo_task_handle);
                count_state_change(old_state, eSuspended);
                update_state_display(eSuspended);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "Resuming Task");
                vTaskResume(state_demo_task_handle);
                count_state_change(old_state, eRunning);
                update_state_display(eRunning);
                suspended = false;
            }
            while (gpio_get_level(BUTTON1_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Button 2 = Give semaphore
        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG, "GIVING SEMAPHORE");
            xSemaphoreGive(demo_semaphore);
            while (gpio_get_level(BUTTON2_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Delete external task after 15 seconds
        if (control_cycle >= 150 && !external_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            vTaskDelete(external_delete_handle);
            count_state_change(eRunning, eDeleted);
            external_deleted = true;
        }

        // Every 5 seconds, show counter
        if (control_cycle % 50 == 0) {
            ESP_LOGI(TAG, "=== STATE CHANGE COUNTER ===");
            for (int i = 0; i < 5; i++) {
                ESP_LOGI(TAG, "%s: %d", state_names[i], state_changes[i]);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// -------------------- External Delete Task --------------------
void external_delete_task(void *pvParameters)
{
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "External delete task running: %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -------------------- Main --------------------
void app_main(void)
{
    ESP_LOGI(TAG, "=== Exercise 1 & 2: State Counter + LED Indicator ===");

    // GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_RUNNING) | (1ULL << LED_READY) |
                        (1ULL << LED_BLOCKED) | (1ULL << LED_SUSPENDED),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON1_PIN) | (1ULL << BUTTON2_PIN),
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&button_conf);

    demo_semaphore = xSemaphoreCreateBinary();
    if (demo_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    xTaskCreate(state_demo_task, "StateDemo", 4096, NULL, 3, &state_demo_task_handle);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, &control_task_handle);
    xTaskCreate(external_delete_task, "ExtDelete", 2048, NULL, 2, &external_delete_handle);

    ESP_LOGI(TAG, "System ready. Press GPIO0 to Suspend/Resume, GPIO15 to Give Semaphore.");
}

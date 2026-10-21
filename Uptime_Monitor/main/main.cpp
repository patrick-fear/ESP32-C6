//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <led_strip.h>
#include <nvs_flash.h>
#include <sdkconfig.h>


// Custom includes
#include "main.hpp"
#include "RGB_LED.hpp"
#include "Settings.h"
#include "TFT_display.hpp"
#include "UptimeMonitor.hpp"
#include "wifi_setup.hpp"


// module name for ESP_LOG
static const char *TAG = "main";


// Use SDK Config Editor to enable these two options and then this will enable additional task report debugging
#if defined(configUSE_TRACE_FACILITY) && defined(configUSE_APPLICATION_TASK_TAG)
    #define ENALBE_DEBUG_REPORT     0

    #define LOOP_MSEC_DEBUG_REPORT  10000
#endif


// Flag to enable hue cycling, if not defined the referts back to rainbow cycling
#define RGB_CYCLE_HUE

// How often to we cycle to the next RGB
#ifdef RGB_CYCLE_HUE
    #define LOOP_MSEC_RBG_LED 10
#else
    #define LOOP_MSEC_RBG_LED 250
#endif  // RGB_CYCLE_HUE

// How often we check the servcies status
#define LOOP_MSEC_SERVICE_CHECK 5000


// FreeRTOS task priority
// Higher number is higher priority
#define TASK_PRIO_3         3
#define TASK_PRIO_2         2

// FreeRTOS queue for TFT display
#define TFT_MESSAGE_QUEUE_LEN 40
QueueHandle_t TFT_Message_Queue;

// Main app name and version constants
const char* MAIN_NAME_LONG = SETTINGS_MAIN_NAME_LONG;
const char* MAIN_NAME_SHORT = SETTINGS_MAIN_NAME_SHORT;
const char* MAIN_VERSION = SETTINGS_MAIN_VERSION;




#ifdef ENALBE_DEBUG_REPORT

/*******************************************************************************
 * @brief           Log out task information for debugging
 *
 * This example demonstrates how a human readable table of run time stats
 * information is generated from raw data provided by uxTaskGetSystemState().
 * The human readable table is written to pcWriteBuffer. (see the vTaskList()
 * API function which actually does just this).
 * Copied from https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/01-uxTaskGetSystemState
 *
 ******************************************************************************/
void vTaskGetRunTimeStats(void)
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize;
    uint32_t x;
    unsigned long ulTotalRunTime, ulStatsAsPercentage;

    // Header line
    ESP_LOGI(TAG, "%4s %-20s %20s %20s %20s %20s %21s",
        "idx",
        "pcTaskName",
        "uxCurrentPriority",
        "eCurrentState",
        "ulRunTimeCounter",
        "usStackHighWaterMark",
        "ulStatsAsPercentage");

    // Take a snapshot of the number of tasks in case it changes while this
    // function is executing.
    uxArraySize = uxTaskGetNumberOfTasks();

    // Allocate a TaskStatus_t structure for each task. An array could be
    // allocated statically at compile time.
#ifdef __cplusplus
    pxTaskStatusArray = static_cast<TaskStatus_t*>(pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) ));
#else
    pxTaskStatusArray = (TaskStatus_t*)pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );
#endif  // __cplusplus

    if (pxTaskStatusArray != NULL) {
        // Generate raw status information about each task.
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray,
            uxArraySize,
            &ulTotalRunTime);

        // For percentage calculations.
        ulTotalRunTime /= 100UL;

        // Avoid divide by zero errors.
        if (ulTotalRunTime > 0) {
            // For each populated position in the pxTaskStatusArray array,
            // format the raw data as human readable ASCII data.
            for (x = 0; x < uxArraySize; x++) {
                // What percentage of the total run time has the task used?
                // This will always be rounded down to the nearest integer.
                // ulTotalRunTimeDiv100 has already been divided by 100.
                ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

                if (ulStatsAsPercentage > 0UL) {
                    ESP_LOGI(TAG, "%4d %-20s %20lu %20lu %20lu %20ld %20lu%%",
                        x,
                        pxTaskStatusArray[x].pcTaskName,
                        pxTaskStatusArray[x].uxCurrentPriority,
                        pxTaskStatusArray[x].eCurrentState,
                        pxTaskStatusArray[x].ulRunTimeCounter,
                        pxTaskStatusArray[x].usStackHighWaterMark,
                        ulStatsAsPercentage);
                } else {
                    // If the percentage is zero here then the task has
                    // consumed less than 1% of the total run time.
                    ESP_LOGI(TAG, "%4d %-20s %20lu %20lu %20lu %20ld %20s%%",
                        x,
                        pxTaskStatusArray[x].pcTaskName,
                        pxTaskStatusArray[x].uxCurrentPriority,
                        pxTaskStatusArray[x].eCurrentState,
                        pxTaskStatusArray[x].ulRunTimeCounter,
                        pxTaskStatusArray[x].usStackHighWaterMark,
                        "<1");
                }
            }
        }

        // The array is no longer needed, free the memory it consumes.
        vPortFree(pxTaskStatusArray);
    }
    //ESP_LOGI(TAG, "");

    return;
}

#endif  // ENALBE_DEBUG_REPORT


/*******************************************************************************
 * @brief           FreeRTOS task for RGB LED
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/
void Task_RGB_LED(void *pvParameters)
{
    while (true) {
        // Task operations
        // Cycle through each color
        #ifdef RGB_CYCLE_HUE
            rgb_led_cycle_Hue();
        #else
            rgb_led_cycle_Color();
        #endif  // RGB_CYCLE_HUE

        //vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
        vTaskDelay(LOOP_MSEC_RBG_LED / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
    return;
}


/*******************************************************************************
 * @brief           FreeRTOS task for TFT display
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/
void Task_TFT_Display(void *pvParameters) {
    TFT_Queue_Data rx_data;  // data type should be same as queue item type
    int to_wait_ms = 5000;  // the maximal blocking waiting time of millisecond, 0 will cause xQueueReceive to return immediately
    const TickType_t xTicksToWait = pdMS_TO_TICKS(to_wait_ms);

    while (true) {
        // Task operations
        if (xQueueReceive(TFT_Message_Queue, (void *)&rx_data, xTicksToWait) == pdTRUE) {
            ESP_LOGD(TAG, "q rx");
            TFT_display_message(rx_data.status, rx_data.string_line_1, rx_data.string_line_2, rx_data.string_line_3, rx_data.string_line_4);
        } else {
            ESP_LOGD(TAG, "q past %d ms", to_wait_ms);
        }

        // the xQueueReceive has a simlar vTaskDelay as it waits, so don't need to run that here
        //vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
        //vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
    return;
}


/*******************************************************************************
 * @brief           FreeRTOS task for UptimeMonitor
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/
void Task_UptimeMonitor(void *pvParameters) {
    int8_t status;
    static int8_t lastStatus = -2;
    TFT_Queue_Data tx_data;
    EventBits_t bits;

    ESP_LOGD(TAG, "Task_UptimeMonitor");

    while (true) {

        // Get event group
        bits = xEventGroupGetBits(s_wifi_event_group);

        // Check the event to see if WiFi is connected or not
        if (bits & WIFI_CONNECTED_BIT) {

            // Task operations
            status = checkServices();

            // Only update the display if the status changes
            if (status != lastStatus) {
                lastStatus = status;
                tx_data.status = status;
                sprintf(tx_data.string_line_1, "%s %s", MAIN_NAME_SHORT, MAIN_VERSION);
                sprintf(tx_data.string_line_2, "SSID: %s", WIFI_SSID);
                sprintf(tx_data.string_line_3, "http://%s", wifi_get_ip_addr());
                sprintf(tx_data.string_line_4, "MAC: %s", wifi_get_mac_addr());

                // Try to add item to queue, fail immediately if queue is full
                if (xQueueGenericSend(TFT_Message_Queue, (void *)&tx_data, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) {
                    ESP_LOGE(TAG, "q full");
                } else {
                    ESP_LOGD(TAG, "q tx");
                }
            }
            //vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
            vTaskDelay(LOOP_MSEC_SERVICE_CHECK / portTICK_PERIOD_MS);

        } else {
            ESP_LOGD(TAG, "UpdateimMonitor disabled");
            // Infinitate wait/delay for the event group
            bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        }
    }

    vTaskDelete(NULL);
    return;
}


/*******************************************************************************
 * @brief           FreeRTOS task for a http server
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/

// Handle for http server
httpd_handle_t server = NULL;

void Task_Server(void * pvParameters) {
    EventBits_t bits;

    // Implement your custom logic here
    ESP_LOGD(TAG, "Task_Server");

    // Now sit in infinte loop and start/stop webserver when WiFI is connected/disconnected
    while (true) {
        // Infinitate wait/delay for the event group
        bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            // TFT message here
            ESP_LOGI(TAG, "WiFi connected");
            if (server == NULL) {
                server = start_webserver();
                ESP_LOGI(TAG, "System ready!");
            }
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT 0x%X", bits);
        }


        // Infinitate wait/delay for the event group
        bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_FAIL_BIT) {
            // TFT message here
            ESP_LOGI(TAG, "WiFi fail");
            if (server) {
                if (stop_webserver(server) == ESP_OK) {
                    server = NULL;
                } else {
                    ESP_LOGE(TAG, "Failed to stop http server");
                }
            }
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT 0x%X", bits);
        }
    }

    // When you're done, call vTaskDelete. Don't forget this!
    vTaskDelete(NULL);
}


/*******************************************************************************
 * @brief           FreeRTOS task for a onetime WiFi setup
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/
void Task_WiFiSetup(void * pvParameters) {

    // Implement your custom logic here

    // Initialize WiFi
    wifi_connect_and_wait();

    //TFT_display_update_status(false);
    TFT_Queue_Data tx_data;
    tx_data.status = -1;

    sprintf(tx_data.string_line_1, "%s %s", MAIN_NAME_SHORT, MAIN_VERSION);
    sprintf(tx_data.string_line_2, "SSID: %s", WIFI_SSID);
    sprintf(tx_data.string_line_3, "https://%s", wifi_get_ip_addr());
    sprintf(tx_data.string_line_4, "MAC: %s", wifi_get_mac_addr());

    if (xQueueGenericSend(TFT_Message_Queue, (void *)&tx_data, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) {
        ESP_LOGE(TAG, "q full");
    } else {
        ESP_LOGD(TAG, "q tx");
    }

    ESP_LOGI(TAG, "Task_WiFiSetup done");

    // When you're done, call vTaskDelete. Don't forget this!
    vTaskDelete(NULL);
}


#ifdef ENALBE_DEBUG_REPORT

/*******************************************************************************
 * @brief           FreeRTOS task for debug reporting
 *
 * @param[inout]    pvParameters Pointer that will be used as the parameter for the task
 ******************************************************************************/
void Task_DebugReport(void *pvParameters) {
    while (true) {
        vTaskGetRunTimeStats();
        vTaskDelay(LOOP_MSEC_DEBUG_REPORT / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
    return;
}

#endif  //ENALBE_DEBUG_REPORT


/*******************************************************************************
 * @brief FreeRTOS main entry point
 *
 ******************************************************************************/
void app_main(void)
{
    ESP_LOGI(TAG, "%s %s", MAIN_NAME_LONG, MAIN_VERSION);
    ESP_LOGI(TAG, "__DATE__ %s", __DATE__);
    ESP_LOGI(TAG, "__TIME__ %s", __TIME__);

    // Any seutp and h/w configuration
    rgb_led_setup();
    TFT_display_setup();

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    //if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //  ESP_ERROR_CHECK(nvs_flash_erase());
    //  ret = nvs_flash_init();
    //}
    ESP_ERROR_CHECK(ret);

    // Initialize filesystem
    initFileSystem();

    // Load saved services
    loadServices();
    ESP_LOGI(TAG, "setup done");

    // This task will always run and at higher prioirty
    xTaskCreate(
        Task_RGB_LED,   // Task function
        "Task_RGB_LED", // Task name
        1024 * 1,       // Stack size
        NULL,           // Parameters
        TASK_PRIO_3,    // Priority
        NULL            // Task handle - return value
    );

    // Queue for TFT messages
    TFT_Message_Queue = xQueueGenericCreate(TFT_MESSAGE_QUEUE_LEN, sizeof(TFT_Queue_Data), queueQUEUE_TYPE_SET);
    if (TFT_Message_Queue == NULL) {
        ESP_LOGE(TAG, "q creation failed");
    }

    xTaskCreate(
        Task_TFT_Display,   // Task function
        "Task_TFT_Display", // Task name
        1024 * 3,           // Stack size
        NULL,               // Parameters
        TASK_PRIO_2,        // Priority
        NULL                // Task handle - return value
    );

    // This taks needs to wait for WiFi setup to complete
    xTaskCreate(
        Task_UptimeMonitor,         // Task function
        "Task_UptimeMonitor",       // Task name
        1024 * 20,                  // Stack size
        NULL,                       // Parameters
        TASK_PRIO_2,                // Priority
        NULL                        // Task handle - return value
    );

    // This task will Wifir for WiFi setup to complate
    xTaskCreate(
        Task_Server,                // Task function
        "Task_Server",              // Task name
        1024 * 2,                   // Stack size
        NULL,                       // Parameters
        TASK_PRIO_2,                // Priority
        NULL                        // Task handle - return value
    );

    // This task will run once and then singal UptimeMonitor task to run
    xTaskCreate(
        Task_WiFiSetup,              // Task function
        "Task_WiFiSetup",           // Task name
        1024 * 4,                   // Stack size
        NULL,                       // Parameters
        TASK_PRIO_2,                // Priority
        NULL                        // Task handle - return value
    );

#ifdef ENALBE_DEBUG_REPORT
    xTaskCreate(
        Task_DebugReport,   // Task function
        "Task_DebugReport", // Task name
        1024 * 3,           // Stack size
        NULL,               // Parameters
        TASK_PRIO_2,        // Priority
        NULL                // Task handle - return value
    );
#endif  // ENALBE_DEBUG_REPORT

    return;
}


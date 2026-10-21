//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdint.h>
#include <stdlib.h>
#include <string>  // For using the std::string class
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include <freertos/FreeRTOS.h>
#include <lwip/inet.h>
#include <ping/ping_sock.h>
#include <netdb.h>
#include <sys/socket.h>
#include "sdkconfig.h"


// Custom includes
#include "ping_command.hpp"


// module name for ESP_LOG
const static char *TAG = "ping";


// Define a structure to hold user data/results
typedef struct {
    int pings_transmitted;
    int pings_received;
    bool finished;
    // Add other relevant data
} ping_user_data_t;


// Flag on how will sync a ping start vs ping end since the end uses a event_handle
#define USE_SEMAPHORE 1


#ifdef USE_SEMAPHORE
    // Declare a handle for the binary semaphore
    SemaphoreHandle_t xBinarySemaphore_Ping;
#endif // USE_SEMAPHORE


/*******************************************************************************
 * @brief           handler for ping success
 *
 * @param[in]        hdl
 * @param[inout]     args
 ******************************************************************************/
static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    //ping_user_data_t *user_data = (ping_user_data_t *)args;  //get callback arguments
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;

    ESP_LOGI(TAG, "ping_success");

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%d bytes from %s icmp_seq=%d ttl=%d time=%d ms", recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
    return;
}


/*******************************************************************************
 * @brief           handler for ping timeout
 *
 * @param[in]        hdl
 * @param[inout]     args
 ******************************************************************************/
static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    //ping_user_data_t *user_data = (ping_user_data_t *)args;  //get callback arguments
    uint16_t seqno;
    ip_addr_t target_addr;

    ESP_LOGD(TAG, "ping_timeout");

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGE(TAG, "From %s icmp_seq=%d timeout", inet_ntoa(target_addr.u_addr.ip4), seqno);
    return;
}


/*******************************************************************************
 * @brief           handler for ping end
 *
 * @param[in]        hdl
 * @param[inout]     args
 ******************************************************************************/
static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ping_user_data_t *user_data = (ping_user_data_t *)args;  //get callback arguments
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    ESP_LOGD(TAG, "ping_end");

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    ESP_LOGI(TAG, "%d packets transmitted, %d received, time %d ms", transmitted, received, total_time_ms);

    user_data->pings_transmitted = transmitted;
    user_data->pings_received = received;
    user_data->finished = true;

    #ifdef USE_SEMAPHORE
        // Give the semaphore to notify the receiver task
        configASSERT(xSemaphoreGive(xBinarySemaphore_Ping) == pdTRUE);
    #endif  // USE_SEMAPHORE
    return;
}


/*******************************************************************************
 * @brief           Run ping command
 *
 * @param[in]       url URL link to ping
 * @param[in]       count number of ping command to run
 *
 * @retval          true when ping returns from url
 * @retval          false when ping fails to ping url or when error occured
 ******************************************************************************/
bool ping(std::string url, uint32_t count)
{
    esp_err_t ret = ESP_OK;
    bool status;
    ESP_LOGD(TAG, "ping");

    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    getaddrinfo(url.c_str(), NULL, &hint, &res);
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);


    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target_addr;          // target IP address
    config.count = count;  // ESP_PING_COUNT_INFINITE will give infinite mode, esp_ping_stop can stop it
    //config.task_stack_size = ESP_TASK_PING_STACK; // Default value from ESP_PING_DEFAULT_CONFIG() is ESP_TASK_PING_STACK


    // Instantiate the user data structure, so that we can get the results back here in this function
    ping_user_data_t my_data = {
        .pings_transmitted = 0,
        .pings_received = 0,
        .finished = false
    };

    #ifdef USE_SEMAPHORE
        // Create the binary semaphore.
        // The semaphore is initially "empty" (taken), meaning the receiver task will block immediately.
        xBinarySemaphore_Ping = xSemaphoreCreateBinary();
    #endif  // USE_SEMAPHORE

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .cb_args = &my_data,  //pass user data structure
        .on_ping_success = test_on_ping_success,
        .on_ping_timeout = test_on_ping_timeout,
        .on_ping_end = test_on_ping_end,
    };


    esp_ping_handle_t handle;
    ESP_GOTO_ON_ERROR(esp_ping_new_session(&config, &cbs, &handle), err, TAG, "Failed to init ping");
    ESP_GOTO_ON_ERROR(esp_ping_start(handle), err, TAG, "Failed to start ping");

    // Wait for the ping session to finish (e.g., using a task notification, semaphore, or simple vTaskDelay)
    // The results will be populated in 'my_data' by the callback functions
    #ifdef USE_SEMAPHORE
        // Attempt to take the semaphore, blocking indefinitely until it's available
        configASSERT(xSemaphoreTake(xBinarySemaphore_Ping, portMAX_DELAY) == pdTRUE);

        // Cleanup and delete once we have the semaphore
        vSemaphoreDelete(xBinarySemaphore_Ping);
    #else
        // Basic look at variable and delay
        while (my_data.finished == false) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    #endif  // USE_SEMAPHORE

    esp_ping_stop(handle);
    ESP_LOGD(TAG, "finished %d", my_data.finished);
    ESP_LOGD(TAG, "count %d", count);
    ESP_LOGD(TAG, "pings_transmitted %d", my_data.pings_transmitted);
    ESP_LOGD(TAG, "pings_received %d", my_data.pings_received);

err:
    esp_ping_delete_session(handle);
    if ( (my_data.finished) && (my_data.pings_transmitted == count) && (my_data.pings_received == count) ) {
        status = true;
    } else {
        status = false;
    }
    ESP_LOGD(TAG, "ping return %d", status);
    return status;
}

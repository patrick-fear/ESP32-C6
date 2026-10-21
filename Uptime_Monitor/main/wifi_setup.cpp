//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdio.h>
#include <string>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <sdkconfig.h>


// Custom includes
#include "main.hpp"
#include "Settings.h"
#include "UptimeMonitor.hpp"
#include "wifi_setup.hpp"


// module name for ESP_LOG
const static char *TAG = "wifi_setup";


// WiFi credentials, need to update these with your network details
const char* WIFI_SSID = SETTINGS_WIFI_SID;              // WiFi SSID
const char* WIFI_PASSWORD = SETTINGS_WIFI_PASSWORD;     // WiFi password

// Maximum of number of WiFi attach attempts before giving up
#define EXAMPLE_ESP_MAXIMUM_RETRY   30

// Variable to store WiFi attach attempt current number
static uint8_t s_retry_num = 0;

// FreeRTOS event group to signal when we are connected/disconnected
EventGroupHandle_t s_wifi_event_group = xEventGroupCreate();

// Global var fow WiFI interface
static esp_netif_t *s_wifi_netif;


/*******************************************************************************
 * @brief           WiFi event handler for connect/disconnect
 *
 * @param[in]       arg
 * @param[in]       event_base
 * @param[in]       event_id
 * @param[in]       event_data
 ******************************************************************************/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "WiFi retry %d", s_retry_num);

            TFT_Queue_Data tx_data;
            tx_data.status = -1;

            sprintf(tx_data.string_line_1, "%s %s", MAIN_NAME_SHORT, MAIN_VERSION);
            sprintf(tx_data.string_line_2, "SSID: %s", WIFI_SSID);
            sprintf(tx_data.string_line_3, "WiFi retry %d", s_retry_num);
            tx_data.string_line_4[0] = '\0';
            if (xQueueGenericSend(TFT_Message_Queue, (void *)&tx_data, portMAX_DELAY, queueSEND_TO_BACK) != pdTRUE) {
                ESP_LOGE(TAG, "q full");
            } else {
                ESP_LOGD(TAG, "q tx");
            }
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


/*******************************************************************************
 * @brief Main procedure to setup and connect to WiFi
 *
 * Look at <USER>\esp\v5.5.1\esp-idf\examples\wifi\getting_started\station\main\station_example_main.c
 * For example of WiFI Station mode.
 *
 * @return esp_err_t
 ******************************************************************************/
esp_err_t wifi_connect_and_wait(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "WiFi setup");
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);

    //s_wifi_event_group = xEventGroupCreate();

    // 1. Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(init_mdns());
    s_wifi_netif = esp_netif_create_default_wifi_sta();

    // 2. Register for Wi-Fi and IP events
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // 3. Configure and start Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SETTINGS_WIFI_SID,
            .password = SETTINGS_WIFI_PASSWORD,
            // Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
            // If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
            // to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            // WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
            //.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            //.sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            //.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // 4. "Polling" equivalent: wait for the connection event
    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened.
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID: %s", SETTINGS_WIFI_SID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s", SETTINGS_WIFI_SID);
        ret = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        ret = ESP_ERR_INVALID_RESPONSE;
    }

    return ret;
}


/*******************************************************************************
 * @brief Get the WiFi IP address
 *
 * @return pointer to string
 ******************************************************************************/
char* wifi_get_ip_addr(void)
{
    esp_netif_ip_info_t ip_info;
    static char str[4*4];  //4 * (3x digit character + 1x trailing colin/EOL character)

    ESP_ERROR_CHECK(esp_netif_get_ip_info(s_wifi_netif, &ip_info));
    //sprintf(str1, "%s", ip2str(&ip_info.ip));
    sprintf(str, IPSTR, IP2STR(&ip_info.ip));
    return str;
}


/*******************************************************************************
 * @brief Get the WiFi MAC address
 *
 * @return pointer to string
 ******************************************************************************/
char* wifi_get_mac_addr(void)
{
    uint8_t mac[8];
    static char str[6 * 3];  //6 * (2x digits character + 1x trailing colin/EOL character)
    ESP_ERROR_CHECK(esp_netif_get_mac(s_wifi_netif, mac));
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return str;
}




#include "esp_netif.h"
#include "lwip/apps/netbiosns.h"
#include "mdns.h"

#define EXAMPLE_MDNS_HOST_NAME      "esp32c6"
#define MDNS_INSTANCE               "esp home web server"


/*******************************************************************************
 * @brief Initialze MDNS
 *
 * @return esp_err_t
 ******************************************************************************/
static esp_err_t initialise_mdns(void)
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(EXAMPLE_MDNS_HOST_NAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE));

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
    return ESP_OK;
}


/*******************************************************************************
 * @brief Main procedure to setup MDNS and NETBIOS
 *
 * @return esp_err_t
 ******************************************************************************/
esp_err_t init_mdns(void) {
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(EXAMPLE_MDNS_HOST_NAME);
    return ESP_OK;
}

//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__WIFI_SETUP_H__)
#define __WIFI_SETUP_H__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <esp_err.h>
#include <esp_netif.h>


// WiFi credentials, need to update these with your network details
extern const char* WIFI_SSID;       // WiFi SSID
extern const char* WIFI_PASSWORD;   // WiFi password

// FreeRTOS event group to signal when we are connected/disconnected
extern EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about two events:
//  - we are connected to the AP with an IP
//  - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT      BIT0    // WiFi connection completed
#define WIFI_FAIL_BIT           BIT1    // WiFi failed to connect after EXAMPLE_ESP_MAXIMUM_RETRY retries


esp_err_t wifi_connect_and_wait(void);
char* wifi_get_ip_addr(void);
char* wifi_get_mac_addr(void);
esp_err_t init_mdns(void);


#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __WIFI_SETUP_H__

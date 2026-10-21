//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__UPTIME_MONITOR_HPP__)
#define __UPTIME_MONITOR_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <esp_err.h>
#include <esp_http_server.h>


esp_err_t initFileSystem(void);
esp_err_t loadServices(void);
esp_err_t saveServices(void);

httpd_handle_t start_webserver(void);
esp_err_t stop_webserver(httpd_handle_t server);
bool checkServices(void);


#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __UPTIME_MONITOR_HPP__

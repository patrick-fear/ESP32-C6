//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__HTTP_COMMAND_HPP__)
#define __HTTP_COMMAND_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>
#include <esp_http_client.h>


esp_http_client_handle_t http_begin(std::string url, int timeout_ms);
int http_get(esp_http_client_handle_t client);
esp_err_t http_getString(esp_http_client_handle_t client, std::string& payload, uint32_t& payload_size);
esp_err_t http_end(esp_http_client_handle_t client);


#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __HTTP_COMMAND_HPP__

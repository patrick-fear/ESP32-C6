//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdint.h>
#include <stdlib.h>
#include <string>   // For using the std::string class
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <sdkconfig.h>


// Custom includes
#include "http_command.hpp"


// module name for ESP_LOG
const static char *TAG = "http";


#define MAX_USER_DATA_BUFFER_SIZE   (1024 * 20)  // Maximum size of the buffer that we will get over the http link.


typedef struct {
    uint32_t bytes_recieved;    // Number of bytes reciecved
    uint32_t buffer_size;       // Number of bytes stored in the buffer
    std::string buffer;         // Buffer of data
    //int content_length;
    bool finish;                // flag to idnciate that the event handler recieved a HTTP_EVENT_ON_FINISH event
} http_user_data_t;

http_user_data_t data_container;



/*******************************************************************************
 * @brief           HTTP event handler
 *
 * @param[inout]    evt pointer to event data
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    http_user_data_t *user_data = (http_user_data_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            // Get the content length here
            //user_data->content_length = esp_http_client_get_content_length(evt->client);
            user_data->bytes_recieved = 0;
            user_data->buffer_size = 0;
            //ESP_LOGD("HTTP_CLIENT", "Content length: %d", user_data->content_length);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            //ESP_LOGW("HTTP_CLIENT", "user_data->buffer_size %d", user_data->buffer_size);
            user_data->bytes_recieved += evt->data_len;
            if (user_data->buffer_size < MAX_USER_DATA_BUFFER_SIZE) {
                user_data->buffer_size += evt->data_len;
                user_data->buffer.append((char *)evt->data, evt->data_len);
            } else {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA exceeded buffer size, bytes_recieved=%d buffer_size=%d", user_data->bytes_recieved, user_data->buffer_size);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH, Total read: %d", user_data->buffer_size);
            user_data->buffer += '\0';
            user_data->finish = true;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
        default:
            break;
    }
    return ESP_OK;
}


/*******************************************************************************
 * @brief           HTTP begin
 *
 * @param[in]       url URL for the HTTP command
 * @param[in]       timeout_ms HTTP timeout in msec
 *
 * @retval          esp_http_client_handle_t
 * @retval          NULL if any errors occured during setup
 ******************************************************************************/
esp_http_client_handle_t http_begin(std::string url, int timeout_ms) {

    ESP_LOGD(TAG, "http_begin %s", url.c_str());

    // Initialze/clear the data
    data_container.bytes_recieved = 0;
    data_container.buffer_size = 0;
    data_container.buffer = "";
    data_container.finish = false;

    // Confiugre the HTTP client
    esp_http_client_config_t config = {
        .url = url.c_str(),
        .host = NULL,  //new
        .port = 0, //new
        .username = NULL,  //new
        .password = NULL,  //new
        .auth_type = HTTP_AUTH_TYPE_NONE,  //new
        .path = NULL,  //new
        .query = NULL,  //new
        //.cert_pem = NULL,  //new
        .cert_der = NULL,  //new
        .cert_len = 0,  //new
        //.client_cert_pem = NULL,  //new
        .client_cert_der = NULL,  //new
        .client_cert_len = 0,  //new
        .client_key_pem = NULL,  //new
        .client_key_len = 0,  //new
        .client_key_password = NULL,  //new
        .client_key_password_len = 0,  //new
        .tls_version = ESP_HTTP_CLIENT_TLS_VER_ANY,  //new
        .user_agent = NULL,  //new
        .method = HTTP_METHOD_GET,
        .timeout_ms = timeout_ms,
        .disable_auto_redirect = false,  //new
        .max_redirection_count = 0,  //new
        .max_authorization_retries = 0,  //new
        // Other optional configurations like event handler, timeout, etc.
        .event_handler = _http_event_handler,
        .transport_type = HTTP_TRANSPORT_UNKNOWN, //new
        .buffer_size = 0,  //new
        .buffer_size_tx = 0,  //new
        .user_data = &data_container, // Pass the address of your data structure
        .is_async = false,  //new
        .use_global_ca_store = false,  //new
        .skip_cert_common_name_check = false,  //new
        .common_name = NULL,  //new
        .crt_bundle_attach = 0,  //new
        .keep_alive_enable = false, ///new
        .keep_alive_idle = 0, ///new default 5
        .keep_alive_interval = 0, ///new default 5
        .keep_alive_count = 0, ///new default 3
        .if_name =  NULL, ///new
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS
        .alpn_protos = NULL,  //new
#endif
#if CONFIG_ESP_TLS_USE_DS_PERIPHERAL
        .ds_data = NULL,  //new
#endif
        .addr_type = HTTP_ADDR_TYPE_UNSPEC, ///new
    };

    // Initialize  the HTTP client
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        // Handle error
        ESP_LOGE(TAG, "http_get failed to Initialize");
        return NULL;
    }
    return client;
}


/*******************************************************************************
 * @brief           HTTP get the stauts code

 * @param[in]       client esp_http_client_handle_t handle

 * @return          HTTP return status code
 ******************************************************************************/
int http_get(esp_http_client_handle_t client) {
    int status_code;
    ESP_LOGD(TAG, "http_get");

    // Preform the HTP GET request
    esp_err_t err = esp_http_client_perform(client);
    //ESP_LOGE(TAG, "esp_http_client_perform");
    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        // Process the response, e.g., read data using esp_http_client_read()
        ESP_LOGI(TAG, "http_get status_code %d", status_code);
        ESP_LOGD(TAG, "http_get data_container.buffer_size %d", data_container.buffer_size);
        //ESP_LOGE(TAG, "http_get content_length %d", data_container.content_length);
    } else {
        // Handle error
        ESP_LOGE(TAG, "http_get failed to get status_code");
        status_code = HttpStatus_InternalError;
    }

    return status_code;
}


/*******************************************************************************
 * @brief           Get the HTTP string payload
 *
 * @param[in]       client esp_http_client_handle_t handle
 * @param[out]      payload pointer to payload from the HTTP command
 * @param[out]      payload_size pointer to byte size of the payload
 *
 * @retval          ESP_OK on success
 * @retval          ESP_ERR_NOT_FINISHED when event handler did not recieve a HTTP_EVENT_ON_FINISH event
 * @retval          ESP_ERR_INVALID_SIZE when the data recived exceeds the buffer size
 ******************************************************************************/
esp_err_t http_getString(esp_http_client_handle_t client, std::string& payload, uint32_t& payload_size) {
    esp_err_t ret = ESP_OK;
    ESP_LOGD(TAG, "http_getString");

    if (data_container.finish == false) {
        ESP_LOGE(TAG, "http_getString finish == false");
        ret = ESP_ERR_NOT_FINISHED;
    }

    if (data_container.bytes_recieved != data_container.buffer_size) {
        ESP_LOGE(TAG, "http_getString bytes_recieved %d", data_container.bytes_recieved);
        ESP_LOGE(TAG, "http_getString buffer_size %d", data_container.buffer_size);
        ret = ESP_ERR_INVALID_SIZE;
    } else {
        ESP_LOGI(TAG, "http_getString buffer_size %d", data_container.buffer_size);
        //ESP_LOGD(TAG, "http_getString buffer %s", data_container.buffer.c_str());
    }
    payload = data_container.buffer;
    payload_size = data_container.buffer_size;

    return ret;
}


/*******************************************************************************
 * @brief           Cleanup and close down HTTP transation
 *
 * @param[in]       client esp_http_client_handle_t handle
 *
 * @retval          ESP_OK on success
 * @retval          ESP_ERR_NOT_FINISHED when event handler did not recieve a HTTP_EVENT_ON_FINISH event
 * @retval          ESP_ERR_INVALID_SIZE when the data recived exceeds the buffer size
 ******************************************************************************/
esp_err_t http_end(esp_http_client_handle_t client) {
    esp_err_t ret = ESP_OK;
    ESP_LOGD(TAG, "http_end");

    if (data_container.finish == false) {
        ESP_LOGE(TAG, "http_getString finish == false");
        ret = ESP_ERR_NOT_FINISHED;
    }

    if (data_container.bytes_recieved != data_container.buffer_size) {
        ESP_LOGE(TAG, "http_getString bytes_recieved %d", data_container.bytes_recieved);
        ESP_LOGE(TAG, "http_getString buffer_size %d", data_container.buffer_size);
        ret = ESP_ERR_INVALID_SIZE;
    } else {
        ESP_LOGI(TAG, "http_getString buffer_size %d", data_container.buffer_size);
        //ESP_LOGD(TAG, "http_getString buffer %s", data_container.buffer.c_str());
    }

    // Clear out and free up any resources
    data_container.bytes_recieved = 0;
    data_container.buffer_size = 0;
    data_container.buffer = "";
    data_container.finish = false;

    // Cleanup the HTTP client
    esp_http_client_cleanup(client);

    return ret;
}

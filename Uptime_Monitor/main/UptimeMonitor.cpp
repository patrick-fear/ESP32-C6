//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdint.h>
#include <stdlib.h> // For rand(), srand(), RAND_MAX
#include <fstream>
#include <iostream>  // For input/output operations like std::cout
#include <string>  // For using the std::string class
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_littlefs.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <sdkconfig.h>


// Custom includes
#include "http_command.hpp"
#include "main.hpp"
#include "ping_command.hpp"
#include "Settings.h"
#include "TFT_display.hpp"
#include "UptimeMonitor.hpp"


// module name for ESP_LOG
const static char *TAG = "uptime";


// Service types
// Right now the behavior for each is rudimentary
// However, you can use this to expand and add services with more complex checks
enum ServiceType {
    TYPE_HOME_ASSISTANT,
    TYPE_JELLYFIN,
    TYPE_HTTP_GET,
    TYPE_PING
};

// Service structure
typedef struct {
    std::string id;
    std::string name;
    ServiceType type;
    std::string host;
    int port;
    std::string path;
    std::string expectedResponse;
    int checkInterval;
    bool isUp;
    unsigned long lastCheck;
    unsigned long lastUptime;
    std::string lastError;
    int secondsSinceLastCheck;
} Service;


// Store up to 20 services
#define MAX_SERVICES                20
Service services[MAX_SERVICES];
int serviceCount = 0;


// Constant file/dir names
const char* JSON_PATH = "/littlefs";  // JSON file path
const char* JSON_FILE = "/littlefs/services.json";  // full JSON file name and path


std::string generateServiceId(void);
bool checkHomeAssistant(Service& service);
bool checkJellyfin(Service& service);
bool checkHttpGet(Service& service);
bool checkPing(Service& service);
std::string getWebPage(void);
std::string getServiceTypeString(ServiceType type);


/*******************************************************************************
 * @brief           Initialized and mount the LittleFS file system
 *
 * @retval          ESP_OK on success
 * @retval          ESP_FAIL on failurer
 ******************************************************************************/
esp_err_t initFileSystem(void) {
    ESP_LOGI(TAG, "Initializing LittleFS");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = JSON_PATH,
        .partition_label = "littlefs",
        .partition = NULL,  // new
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = true,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}


/*******************************************************************************
 * @brief           handler for webpage
 *
 * @param[inout]    reg
 *
 * @retval          ESP_OK on success
 ******************************************************************************/
static esp_err_t webpage_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "webpage_handler");
    std::string buf = getWebPage();
    const char* resp_str = buf.c_str();

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    //const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/*******************************************************************************
 * @brief           URL handler for webpage
 * ******************************************************************************/
static const httpd_uri_t webpage = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = webpage_handler,
    .user_ctx  = NULL
};


/*******************************************************************************
 * @brief           handler for get services
 *
 * @param[inout]    reg
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t get_servcies_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "get_servcies_handler");
    std::string buf;

    JsonDocument doc;
    JsonArray array = doc["services"].to<JsonArray>();

    unsigned long currentTime = esp_timer_get_time() / 1000ULL; // Divide by 1000 to convert microseconds to milliseconds

    for (int i = 0; i < serviceCount; i++) {
        if (services[i].lastCheck > 0) {
            services[i].secondsSinceLastCheck = (currentTime - services[i].lastCheck) / 1000;
        } else {
            services[i].secondsSinceLastCheck = -1; // Never checked
        }

        JsonObject obj = array.add<JsonObject>();
        obj["id"] = services[i].id;
        obj["name"] = services[i].name;
        obj["type"] = getServiceTypeString(services[i].type);
        obj["host"] = services[i].host;
        obj["port"] = services[i].port;
        obj["path"] = services[i].path;
        obj["expectedResponse"] = services[i].expectedResponse;
        obj["checkInterval"] = services[i].checkInterval;
        obj["isUp"] = services[i].isUp;
        obj["secondsSinceLastCheck"] = services[i].secondsSinceLastCheck;
        obj["lastError"] = services[i].lastError;
    }

    std::string response;
    serializeJson(doc, response);

    const char* resp_str = response.c_str();

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    //const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/*******************************************************************************
 * @brief           URL handler for get service
 * ******************************************************************************/
static const httpd_uri_t get_servcies = {
    .uri       = "/api/services",
    .method    = HTTP_GET,
    .handler   = get_servcies_handler,
    .user_ctx  = NULL
};


/*******************************************************************************
 * @brief           handler to add service
 *
 * @param[inout]    reg
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t add_services_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "add_services_handler");

    int total_len = req->content_len;
    ESP_LOGD(TAG, "total_len=%d", total_len);

    //std::string s1(32, "x");
    char buff[total_len+4];

    int received = 0;
    received = httpd_req_recv(req, buff, total_len);
    if (received <= 0) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"Failed to post control value\"}");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "buff=%s", buff);

    if (serviceCount >= MAX_SERVICES) {
        //request->send(400, "application/json", "{\"error\":\"Maximum services reached\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"Maximum services reached\"}");
        return ESP_FAIL;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buff, total_len);

    if (error) {
        //request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"Invalid JSON\"}");
        return ESP_FAIL;
    }

    Service newService;
    newService.id = generateServiceId();
    newService.name = doc["name"].as<std::string>();

    std::string typeStr = doc["type"].as<std::string>();
    if (typeStr == "home_assistant") {
        newService.type = TYPE_HOME_ASSISTANT;
    } else if (typeStr == "jellyfin") {
        newService.type = TYPE_JELLYFIN;
    } else if (typeStr == "http_get") {
        newService.type = TYPE_HTTP_GET;
    } else if (typeStr == "ping") {
        newService.type = TYPE_PING;
    } else {
        //request->send(400, "application/json", "{\"error\":\"Invalid service type\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"Invalid service type\"}");
        return ESP_FAIL;
    }

    newService.host = doc["host"].as<std::string>();
    newService.port = doc["port"] | 80;
    newService.path = doc["path"] | "/";
    newService.expectedResponse = doc["expectedResponse"] | "*";
    newService.checkInterval = doc["checkInterval"] | 60;
    newService.isUp = false;
    newService.lastCheck = 0;
    newService.lastUptime = 0;
    newService.lastError = "";
    newService.secondsSinceLastCheck = -1;

    ESP_LOGD(TAG, "newService.id=%s", newService.id.c_str());
    ESP_LOGD(TAG, "newService.name=%s", newService.name.c_str());
    ESP_LOGD(TAG, "newService.host=%s", newService.host.c_str());
    ESP_LOGD(TAG, "newService.port=%d", newService.port);
    ESP_LOGD(TAG, "newService.path=%s", newService.path.c_str());
    ESP_LOGD(TAG, "newService.expectedResponse=%s", newService.expectedResponse);
    ESP_LOGD(TAG, "newService.checkInterval=%d", newService.checkInterval);

    services[serviceCount++] = newService;
    saveServices();

    JsonDocument response;
    response["success"] = true;
    response["id"] = newService.id;

    std::string responseStr;
    serializeJson(response, responseStr);
    //request->send(200, "application/json", responseStr);

    const char* resp_str = responseStr.c_str();

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    //const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/*******************************************************************************
 * @brief           URL handler for add service
 * ******************************************************************************/
static const httpd_uri_t add_services = {
    .uri       = "/api/services",
    .method    = HTTP_POST,
    .handler   = add_services_handler,
    .user_ctx  = NULL
};


/*******************************************************************************
 * @brief           handler to delete service
 *
 * @param[inout]    reg
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t delete_services_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "delete_services_handler");

    std::string path;
    std::string serviceId;
    path = req->uri;
    serviceId = path.substr(path.find_last_of('/') + 1);

    ESP_LOGI(TAG, "path=%s", path.c_str());
    ESP_LOGI(TAG, "serviceId=%s", serviceId.c_str());

    int foundIndex = -1;
    for (int i = 0; i < serviceCount; i++) {
        if (services[i].id == serviceId) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) {
        //request->send(404, "application/json", "{\"error\":\"Service not found\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"Service not found\"}");
        return ESP_FAIL;
    }

    // Shift services array
    for (int i = foundIndex; i < serviceCount - 1; i++) {
        services[i] = services[i + 1];
    }
    serviceCount--;

    saveServices();


    //request->send(200, "application/json", "{\"success\":true}");
    std::string responseStr = "{\"success\":true}";
    //request->send(200, "application/json", responseStr);
    //const char* resp_str = responseStr.c_str();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, responseStr.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/*******************************************************************************
 * @brief           URL handler for delete service
 * ******************************************************************************/
static const httpd_uri_t delete_services = {
    .uri       = "/api/services/*",
    .method    = HTTP_DELETE,
    .handler   = delete_services_handler,
    .user_ctx  = NULL
};


#ifdef ENABLE_FAVICON


/*******************************************************************************
 * @brief           handler for Favion
 *
 * @param[inout]    reg
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t favicon_get_handler(httpd_req_t *req) {
    ESP_LOGE(TAG, "favicon");

#ifndef FACICON_LITTLEFS
    FILE *f = fopen("/littlefs/favicon.ico", "rb"); // Adjust path based on your file system
    if (f == NULL) {
        //httpd_resp_send_404(req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open favicon");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/x-icon");
    char chunk[512];
    size_t read_bytes;
    do {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        httpd_resp_send_chunk(req, chunk, read_bytes);
    } while (read_bytes > 0);

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // End chunked transfer

#else

    // https://base64.guru/converter/encode/image/ico
    // Convert your favicon.ico to a Base64 encoded string. Online tools can assist with this.
    // Embed the Base64 string directly within the <head> section of your HTML, using a data: URI:
    // responseStr = <link rel="shortcut icon" href="data:image/x-icon;base64,AAABAAEAAQEAAAEAIAAwAAAAFgAAACgAAAABAAAAAgAAAAEAIAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAP//AAAAAA==" />
    //std::string html = R"rawliteral(<link rel="shortcut icon" href="data:image/x-icon;base64,AAABAAEAAQEAAAEAIAAwAAAAFgAAACgAAAABAAAAAgAAAAEAIAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAP//AAAAAA==" />)rawliteral";
    std::string html = R"rawliteral(<link rel="shortcut icon" href="data:image/x-icon;base64,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" />)rawliteral";
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, html.c_str(), HTTPD_RESP_USE_STRLEN);
#endif
    return ESP_OK;
}

/*******************************************************************************
 * @brief           URL handler for Favicon
 * ******************************************************************************/
httpd_uri_t favicon_uri = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL
};
#endif  // ENABLE_FAVICON


/*******************************************************************************
 * @brief           Main function to start webserver and assign URL handlers
 *
 * @param[inout]    reg
 *
 * @retval          httpd_handle_t
 * @retval          NULL when fails to setup start webserver
 ******************************************************************************/
httpd_handle_t start_webserver(void)
{
    ESP_LOGI(TAG, "start_webserver");

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a privileged user in linux.
    // So when a unprivileged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unprivileged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;  // Enable wildcard matching for the server
    config.stack_size = (1024 * 8);  // Defaule value from HTTPD_DEFAULT_CONFIG() is 4096

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &webpage);
        httpd_register_uri_handler(server, &get_servcies);
        httpd_register_uri_handler(server, &add_services);
        httpd_register_uri_handler(server, &delete_services);
#ifdef ENABLE_FAVICON
        // W (76604) httpd_uri: httpd_uri: URI '/favicon.ico' not found
        // W (76604) httpd_txrx: httpd_resp_send_err: 404 Not Found - Nothing matches the given URI
        httpd_register_uri_handler(server, &favicon_uri);
#endif  // ENABLE_FAVICON
#if CONFIG_EXAMPLE_ENABLE_SSE_HANDLER
        httpd_register_uri_handler(server, &sse); // Register SSE handler
#endif
#if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
#endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


/*******************************************************************************
 * @brief           Main function to stop webserver
 *
 * @param[in]       server handler to server
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t stop_webserver(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Server down");

    // Stop the httpd server
    return httpd_stop(server);
}


/*******************************************************************************
 * @brief           Get random number within a min/max range
 *
 * Equivalent to Arduino random(min, max) -> range [min, max-1]
 *
 * @param[in]       min minumum number
 * @param[in]       max maximum number
 *
 * @return          random number
 ******************************************************************************/
int32_t random_range(int32_t min, int32_t max) {
    if (min >= max) return min; // Handle invalid range
    int32_t range = max - min;
    // Use modulo for range mapping, adding min as an offset
    return min + (int32_t)(esp_random() % abs(range));
}


/*******************************************************************************
 * @brief           Generate a unique system ID
 *
 * The value generated is merge of current time and a random number
 *
 * @return          std::string
 ******************************************************************************/
std::string generateServiceId(void) {
    unsigned long currentTime = esp_timer_get_time() / 1000ULL; // Divide by 1000 to convert microseconds to milliseconds
    //return std::string(currentTime) + std::string(random_range(1000, 9999));
    std::string ret;
    ret = std::to_string(currentTime) + std::to_string(random_range(1000, 9999));
    ESP_LOGD(TAG, "generateServiceId=%s", ret.c_str());
    return ret;
}


/*******************************************************************************
 * @brief           Run all the service checks if they are past their time
 *
 * @retval          true if all servcies are passing
 * @retval          false if any service fails
 ******************************************************************************/
bool checkServices(void) {
    static bool force = true;
    unsigned long currentTime = esp_timer_get_time() / 1000ULL; // Divide by 1000 to convert microseconds to milliseconds

    for (int i = 0; i < serviceCount; i++) {
        // Check if it's time to check this service
        if ( (currentTime - services[i].lastCheck < services[i].checkInterval * 1000) && (!force)) {
            continue;
        }

        services[i].lastCheck = currentTime;
        bool wasUp = services[i].isUp;

        switch (services[i].type) {
            case TYPE_HOME_ASSISTANT:
                services[i].isUp = checkHomeAssistant(services[i]);
                break;
            case TYPE_JELLYFIN:
                services[i].isUp = checkJellyfin(services[i]);
                break;
            case TYPE_HTTP_GET:
                services[i].isUp = checkHttpGet(services[i]);
                break;
            case TYPE_PING:
                services[i].isUp = checkPing(services[i]);
                break;
        }

        if (services[i].isUp) {
            services[i].lastUptime = currentTime;
            services[i].lastError = "";
        }

        // Log status changes
        if (wasUp != services[i].isUp) {
            ESP_LOGI(TAG, "Service '%s' is now %s",
                services[i].name.c_str(),
                services[i].isUp ? "UP" : "DOWN");
        }
    }

    // Check for return status
    for (int i = 0; i < serviceCount; i++) {
        if (!services[i].isUp) {
            return false;
        }
    }
    force = false;
    return true;
}


/*******************************************************************************
 * @brief           Check service for Home Assistant
 *
 * Technically just detectes any endpoint, so would be good to support auth and check if it's actually home assistant
 * could parse /api/states or something to check there are valid entities and that it's actually HA
 *
 * @param[inout]    service
 *
 * @return          true for passing, false for failing
 ******************************************************************************/
bool checkHomeAssistant(Service& service) {
    std::string url = "http://" + service.host + ":" + std::to_string(service.port) + "/api/";
    ESP_LOGD(TAG, "checkHomeAssistant %s", url.c_str());

    esp_http_client_handle_t client = http_begin(url, 15000);
    int httpCode = http_get(client);
    bool isUp = false;

    if (httpCode > 0) {
        // HA returns 404 for /api/, but ANY positive HTTP status means the service is alive
        isUp = true;
    } else {
        service.lastError = "Connection failed: " + std::to_string(httpCode);
    }

    http_end(client);
    ESP_LOGD(TAG, "checkHomeAssistant isUp %d", isUp);
    return isUp;
}



/*******************************************************************************
 * @brief           Check service for Jelly Fish
 *
 * @param[inout]    service
 *
 * @return          true for passing, false for failing
 ******************************************************************************/
bool checkJellyfin(Service& service) {
    std::string url = "http://" + service.host + ":" + std::to_string(service.port) + "/health";
    ESP_LOGD(TAG, "checkJellyfin %s", url.c_str());

    esp_http_client_handle_t client = http_begin(url, 15000);
    int httpCode = http_get(client);
    bool isUp = false;

    if (httpCode > 0) {
        if (httpCode == HttpStatus_Ok) {
            isUp = true;
        }
    } else {
        service.lastError = "Connection failed: " + std::to_string(httpCode);
    }

    http_end(client);
    ESP_LOGD(TAG, "checkJellyfin isUp %d", isUp);
    return isUp;
}


/*******************************************************************************
 * @brief           Check service with HTTP_GET command
 *
 * @param[inout]    service
 *
 * @return          true for passing, false for failing
 ******************************************************************************/
bool checkHttpGet(Service& service) {
    std::string url = "http://" + service.host + ":" + std::to_string(service.port) + service.path;
    ESP_LOGD(TAG, "checkHttpGetr %s", url.c_str());

    esp_http_client_handle_t client = http_begin(url, 15000);
    int httpCode = http_get(client);
    bool isUp = false;

    if (httpCode > 0) {
        // Look to use defined values
        // 200: HttpStatus_Ok
        // 301: HttpStatus_MovedPermanently
        // 302: HttpStatus_Found
        // 307: HttpStatus_TemporaryRedirect
        if (httpCode == HttpStatus_Ok || httpCode == HttpStatus_Found) {
            if (service.expectedResponse == "*") {
                isUp = true;
            } else {
                std::string payload;
                esp_err_t ret;
                uint32_t payload_size;
                ret = http_getString(client, payload, payload_size);
                if (ret == ESP_OK) {
                    size_t pos_sub = payload.find(service.expectedResponse);
                    if (pos_sub == std::string::npos) {
                        isUp = false;
                        service.lastError = "Response mismatch";
                    } else {
                        isUp = true;
                    }
                } else {
                    isUp = false;
                    service.lastError = "Response payload max limit";
                }
            }
        } else {
            service.lastError = "HTTP " + std::to_string(httpCode);
        }
    } else {
        service.lastError = "Connection failed: " + std::to_string(httpCode);
    }

    http_end(client);
    ESP_LOGD(TAG, "checkHttpGetr isUp %d", isUp);
    return isUp;
}


/*******************************************************************************
 * @brief           Check service wtih ping command
 *
 * @param[inout]    service
 *
 * @return          true for passing, false for failing
 ******************************************************************************/
bool checkPing(Service& service) {
    ESP_LOGD(TAG, "checkPing %s", service.host.c_str());

    bool success = ping(service.host, 3);
    if (!success) {
        service.lastError = "Ping timeout";
    }
    ESP_LOGD(TAG, "checkPing success %d", success);
    return success;
}


/*******************************************************************************
 * @brief           Save the servcies in memory to the LittleFS file system
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t saveServices(void) {

    ESP_LOGI(TAG, "saveServices");

    ESP_LOGD(TAG, "Opening file");
    std::ofstream file(JSON_FILE);
    if (!file.is_open()) {
        ESP_LOGE(TAG, "Failed to open %s for writing", JSON_FILE);
        return ESP_FAIL;
    }

    JsonDocument doc;
    JsonArray array = doc["services"].to<JsonArray>();

    for (int i = 0; i < serviceCount; i++) {
        JsonObject obj = array.add<JsonObject>();
        obj["id"] = services[i].id;
        obj["name"] = services[i].name;
        obj["type"] = (int)services[i].type;
        obj["host"] = services[i].host;
        obj["port"] = services[i].port;
        obj["path"] = services[i].path;
        obj["expectedResponse"] = services[i].expectedResponse;
        obj["checkInterval"] = services[i].checkInterval;
    }

    serializeJson(doc, file);
    file.close();
    ESP_LOGI(TAG, "Services saved");
    return ESP_OK;
}


/*******************************************************************************
 * @brief           Loat servcies from the LittleFS into memory
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t loadServices(void) {

    ESP_LOGI(TAG, "loadServices");

    ESP_LOGD(TAG, "Opening file");
    std::ifstream file(JSON_FILE);
    if (!file.is_open()) {
        ESP_LOGE(TAG, "No %s found, starting fresh", JSON_FILE);
        return ESP_FAIL;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        ESP_LOGE(TAG, "Failed to parse %s", JSON_FILE);
        return ESP_FAIL;
    }

    JsonArray array = doc["services"];
    serviceCount = 0;

    for (JsonObject obj : array) {
        if (serviceCount >= MAX_SERVICES) {
            break;
        }

        services[serviceCount].id = obj["id"].as<std::string>();
        services[serviceCount].name = obj["name"].as<std::string>();
        services[serviceCount].type = (ServiceType)obj["type"].as<int>();
        services[serviceCount].host = obj["host"].as<std::string>();
        services[serviceCount].port = obj["port"];
        services[serviceCount].path = obj["path"].as<std::string>();
        services[serviceCount].expectedResponse = obj["expectedResponse"].as<std::string>();
        services[serviceCount].checkInterval = obj["checkInterval"];
        services[serviceCount].isUp = false;
        services[serviceCount].lastCheck = 0;
        services[serviceCount].lastUptime = 0;
        services[serviceCount].lastError = "";
        services[serviceCount].secondsSinceLastCheck = -1;

        serviceCount++;
    }

    ESP_LOGI(TAG, "Loaded %d services", serviceCount);
    return ESP_OK;
}


/*******************************************************************************
 * @brief           Convert ServiceType to a std::string
 *
 * @param[in]       type ServiceType
 *
 * @return          std::srring value for the service
 ******************************************************************************/
std::string getServiceTypeString(ServiceType type) {
    switch (type) {
        case TYPE_HOME_ASSISTANT: return "home_assistant";
        case TYPE_JELLYFIN: return "jellyfin";
        case TYPE_HTTP_GET: return "http_get";
        case TYPE_PING: return "ping";
        default: return "unknown";
    }
}


/*******************************************************************************
 * @brief           Get HTML raw string for the webpage
 *
 * @return          std::string
 ******************************************************************************/
std::string getWebPage(void) {
  std::string html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)rawliteral";
    html += MAIN_NAME_LONG;
    html += R"rawliteral( )rawliteral";
    html += MAIN_VERSION;
    html += R"rawliteral(</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
        }

        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }

        .card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .add-service-form {
            display: grid;
            gap: 15px;
        }

        .form-group {
            display: flex;
            flex-direction: column;
        }

        .form-row {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }

        label {
            font-weight: 600;
            margin-bottom: 5px;
            color: #333;
            font-size: 0.9em;
        }

        input, select {
            padding: 10px;
            border: 2px solid #e0e0e0;
            border-radius: 6px;
            font-size: 1em;
            transition: border-color 0.3s;
        }

        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }

        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 6px;
            font-size: 1em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }

        .btn-primary {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }

        .btn-primary:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
        }

        .btn-danger {
            background: #ef4444;
            color: white;
            padding: 8px 16px;
            font-size: 0.9em;
        }

        .btn-danger:hover {
            background: #dc2626;
        }

        .services-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
        }

        .service-card {
            background: white;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            border-left: 4px solid #e0e0e0;
            transition: all 0.3s;
        }

        .service-card.up {
            border-left-color: #10b981;
        }

        .service-card.down {
            border-left-color: #ef4444;
        }

        .service-card:hover {
            transform: translateY(-4px);
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }

        .service-header {
            display: flex;
            justify-content: space-between;
            align-items: start;
            margin-bottom: 15px;
        }

        .service-name {
            font-size: 1.2em;
            font-weight: 700;
            color: #1f2937;
        }

        .service-status {
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: 600;
        }

        .service-status.up {
            background: #d1fae5;
            color: #065f46;
        }

        .service-status.down {
            background: #fee2e2;
            color: #991b1b;
        }

        .service-info {
            margin-bottom: 10px;
            color: #6b7280;
            font-size: 0.9em;
        }

        .service-info strong {
            color: #374151;
        }

        .service-actions {
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px solid #e5e7eb;
        }

        .type-badge {
            display: inline-block;
            padding: 4px 10px;
            background: #e0e7ff;
            color: #3730a3;
            border-radius: 6px;
            font-size: 0.8em;
            font-weight: 600;
            margin-bottom: 10px;
        }

        .empty-state {
            text-align: center;
            padding: 60px 20px;
            color: white;
        }

        .empty-state h3 {
            font-size: 1.5em;
            margin-bottom: 10px;
        }

        .hidden {
            display: none;
        }

        .alert {
            padding: 12px 20px;
            border-radius: 6px;
            margin-bottom: 20px;
        }

        .alert-success {
            background: #d1fae5;
            color: #065f46;
        }

        .alert-error {
            background: #fee2e2;
            color: #991b1b;
        }

        @media (max-width: 768px) {
            .form-row {
                grid-template-columns: 1fr;
            }

            .services-grid {
                grid-template-columns: 1fr;
            }

            .header h1 {
                font-size: 1.8em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>)rawliteral";
    html += MAIN_NAME_LONG;
    html += R"rawliteral( )rawliteral";
    html += MAIN_VERSION;
    html += R"rawliteral(</h1>
            <p>Monitor your services and infrastructure health</p>
        </div>

        <div id="alertContainer"></div>

        <div class="card">
            <h2 style="margin-bottom: 20px; color: #1f2937;">Add New Service</h2>
            <form id="addServiceForm" class="add-service-form">
                <div class="form-group">
                    <label for="serviceName">Service Name</label>
                    <input type="text" id="serviceName" required placeholder="My Service">
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="serviceType">Service Type</label>
                        <select id="serviceType" required>
                            <option value="home_assistant">Home Assistant</option>
                            <option value="jellyfin">Jellyfin</option>
                            <option value="http_get">HTTP GET</option>
                            <option value="ping">Ping</option>
                        </select>
                    </div>

                    <div class="form-group">
                        <label for="serviceHost">Host / IP Address</label>
                        <input type="text" id="serviceHost" required placeholder="192.168.1.100">
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="servicePort">Port</label>
                        <input type="number" id="servicePort" value="80" required>
                    </div>

                    <div class="form-group">
                        <label for="checkInterval">Check Interval (seconds)</label>
                        <input type="number" id="checkInterval" value="60" required min="10">
                    </div>
                </div>

                <div class="form-group" id="pathGroup">
                    <label for="servicePath">Path</label>
                    <input type="text" id="servicePath" value="/" placeholder="/">
                </div>

                <div class="form-group" id="responseGroup">
                    <label for="expectedResponse">Expected Response (* for any)</label>
                    <input type="text" id="expectedResponse" value="*" placeholder="*">
                </div>

                <button type="submit" class="btn btn-primary">Add Service</button>
            </form>
        </div>

        <h2 style="color: white; margin-bottom: 20px; font-size: 1.5em;">Monitored Services</h2>
        <div id="servicesContainer" class="services-grid"></div>
        <div id="emptyState" class="empty-state hidden">
            <h3>No services yet</h3>
            <p>Add your first service using the form above</p>
        </div>
    </div>

    <script>
        let services = [];

        // Update form fields based on service type
        document.getElementById('serviceType').addEventListener('change', function() {
            const type = this.value;
            const pathGroup = document.getElementById('pathGroup');
            const responseGroup = document.getElementById('responseGroup');
            const portInput = document.getElementById('servicePort');

            if (type === 'ping') {
                pathGroup.classList.add('hidden');
                responseGroup.classList.add('hidden');
            } else {
                pathGroup.classList.remove('hidden');

                if (type === 'http_get') {
                    responseGroup.classList.remove('hidden');
                } else {
                    responseGroup.classList.add('hidden');
                }

                // Set default ports
                // Big benefit of the defined types is we can set defaults like these
                if (type === 'home_assistant') {
                    portInput.value = 8123;
                } else if (type === 'jellyfin') {
                    portInput.value = 8096;
                } else {
                    portInput.value = 80;
                }
            }
        });

        // Add service
        document.getElementById('addServiceForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const data = {
                name: document.getElementById('serviceName').value,
                type: document.getElementById('serviceType').value,
                host: document.getElementById('serviceHost').value,
                port: parseInt(document.getElementById('servicePort').value),
                path: document.getElementById('servicePath').value,
                expectedResponse: document.getElementById('expectedResponse').value,
                checkInterval: parseInt(document.getElementById('checkInterval').value)
            };

            try {
                const response = await fetch('/api/services', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    showAlert('Service added successfully!', 'success');
                    this.reset();
                    document.getElementById('serviceType').dispatchEvent(new Event('change'));
                    loadServices();
                } else {
                    showAlert('Failed to add service', 'error');
                }
            } catch (error) {
                showAlert('Error: ' + error.message, 'error');
            }
        });

        // Load services
        async function loadServices() {
            try {
                const response = await fetch('/api/services');
                const data = await response.json();
                services = data.services || [];
                renderServices();
            } catch (error) {
                console.error('Error loading services:', error);
            }
        }

        // Render services
        function renderServices() {
            const container = document.getElementById('servicesContainer');
            const emptyState = document.getElementById('emptyState');

            if (services.length === 0) {
                container.innerHTML = '';
                emptyState.classList.remove('hidden');
                return;
            }

            emptyState.classList.add('hidden');

            container.innerHTML = services.map(service => {
                let uptimeStr = 'Not checked yet';

                if (service.secondsSinceLastCheck >= 0) {
                    const seconds = service.secondsSinceLastCheck;
                    if (seconds < 60) {
                        uptimeStr = `${seconds}s ago`;
                    } else if (seconds < 3600) {
                        const minutes = Math.floor(seconds / 60);
                        const secs = seconds % 60;
                        uptimeStr = `${minutes}m ${secs}s ago`;
                    } else {
                        const hours = Math.floor(seconds / 3600);
                        const minutes = Math.floor((seconds % 3600) / 60);
                        uptimeStr = `${hours}h ${minutes}m ago`;
                    }
                }

                return `
                    <div class="service-card ${service.isUp ? 'up' : 'down'}">
                        <div class="service-header">
                            <div>
                                <div class="service-name">${service.name}</div>
                                <div class="type-badge">${service.type.replace('_', ' ').toUpperCase()}</div>
                            </div>
                            <span class="service-status ${service.isUp ? 'up' : 'down'}">
                                ${service.isUp ? 'UP' : 'DOWN'}
                            </span>
                        </div>
                        <div class="service-info">
                            <strong>Host:</strong> ${service.host}:${service.port}
                        </div>
                        ${service.path && service.type !== 'ping' ? `
                        <div class="service-info">
                            <strong>Path:</strong> ${service.path}
                        </div>
                        ` : ''}
                        <div class="service-info">
                            <strong>Check Interval:</strong> ${service.checkInterval}s
                        </div>
                        <div class="service-info">
                            <strong>Last Check:</strong> ${uptimeStr}
                        </div>
                        ${service.lastError ? `
                        <div class="service-info" style="color: #ef4444;">
                            <strong>Error:</strong> ${service.lastError}
                        </div>
                        ` : ''}
                        <div class="service-actions">
                            <button class="btn btn-danger" onclick="deleteService('${service.id}')">Delete</button>
                        </div>
                    </div>
                `;
            }).join('');
        }

        // Delete service
        async function deleteService(id) {
            if (!confirm('Are you sure you want to delete this service?')) {
                return;
            }

            try {
                const response = await fetch(`/api/services/${id}`, {
                    method: 'DELETE'
                });

                if (response.ok) {
                    showAlert('Service deleted successfully', 'success');
                    loadServices();
                } else {
                    showAlert('Failed to delete service', 'error');
                }
            } catch (error) {
                showAlert('Error: ' + error.message, 'error');
            }
        }

        // Show alert
        function showAlert(message, type) {
            const container = document.getElementById('alertContainer');
            const alert = document.createElement('div');
            alert.className = `alert alert-${type}`;
            alert.textContent = message;
            container.appendChild(alert);

            setTimeout(() => {
                alert.remove();
            }, 3000);
        }

        // Auto-refresh services every 5 seconds
        setInterval(loadServices, 5000);

        // Initial load
        loadServices();
        document.getElementById('serviceType').dispatchEvent(new Event('change'));
    </script>
</body>
</html>
)rawliteral";
    return html;
}

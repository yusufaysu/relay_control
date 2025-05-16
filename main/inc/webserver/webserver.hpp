#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include <algorithm>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

class WebServer {
public:
    WebServer();
    esp_err_t begin();
private:
    static esp_err_t login_handler(httpd_req_t *req);
    static esp_err_t panel_handler(httpd_req_t *req);
    static esp_err_t file_handler(httpd_req_t *req);
    static esp_err_t redirect_to_login(httpd_req_t *req);
    static esp_err_t toggle_handler(httpd_req_t *req);
    httpd_handle_t server;
};
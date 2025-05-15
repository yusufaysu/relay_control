#include "webserver.hpp"

#define TAG "WebServer"

WebServer::WebServer() : server(NULL) {}

esp_err_t WebServer::login_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) return ESP_FAIL;

    buf[ret] = '\0';

    char username[32] = {0};
    char password[32] = {0};

    httpd_query_key_value(buf, "username", username, sizeof(username));
    httpd_query_key_value(buf, "password", password, sizeof(password));

    ESP_LOGI("WebServer", "Login denemesi: kullanıcı=%s, şifre=%s", username, password);

    if (strcmp(username, "admin") == 0 && strcmp(password, "1234") == 0) {
        httpd_resp_sendstr(req, "OK");  // JS tarafından kontrol edilecek
    } else {
        httpd_resp_sendstr(req, "FAIL");
    }

    return ESP_OK;
}

esp_err_t WebServer::file_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs";
    strlcat(filepath, req->uri, sizeof(filepath));

    ESP_LOGI("WebServer", "file_handler çağrıldı: %s", filepath);

    FILE *file = fopen(filepath, "r");
    if (!file) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Dosya bulunamadı");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");  // Türkçe karakterler için önemli

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        httpd_resp_send_chunk(req, line, strlen(line));
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::begin() {
    // SPIFFS dosya sistemi mount et
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",  // partitions.csv ile aynı label olmalı
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("WebServer", "SPIFFS init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI("WebServer", "SPIFFS initialized");

    // Web sunucuyu başlat
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE("WebServer", "HTTP sunucusu başlatılamadı!");
        return ESP_FAIL;
    }
    ESP_LOGI("WebServer", "HTTP sunucusu başlatıldı");

    // URI: /login.html (SPIFFS'ten sunulacak)
    httpd_uri_t login_html = {
        .uri      = "/login.html",
        .method   = HTTP_GET,
        .handler  = file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &login_html);

    // URI: /favicon.ico (tarayıcı otomatik ister, varsa SPIFFS'ten sunar)
    httpd_uri_t favicon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &favicon);

    // URI: /login (formdan gelen POST isteği)
    httpd_uri_t login_post = {
        .uri      = "/login",
        .method   = HTTP_POST,
        .handler  = login_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &login_post);

    ESP_LOGI("WebServer", "Tüm URI handler'ları kaydedildi");
    return ESP_OK;
}
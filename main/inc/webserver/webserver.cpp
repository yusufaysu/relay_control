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

esp_err_t WebServer::redirect_to_login(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/login.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::toggle_handler(httpd_req_t *req) {
    static bool relay_on = false;
    relay_on = !relay_on;

    // Buraya gerçek GPIO çıkış kodu ekleyebilirsin:
    // gpio_set_level(RELAY_GPIO, relay_on ? 1 : 0);

    const char *status = relay_on ? "Açık" : "Kapalı";
    ESP_LOGI("WebServer", "Röle durumu: %s", status);

    httpd_resp_sendstr(req, status);
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

    // URI: /
    httpd_uri_t root_redirect = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = redirect_to_login,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_redirect);

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

    httpd_uri_t toggle_uri = {
        .uri      = "/toggle",
        .method   = HTTP_POST,
        .handler  = toggle_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &toggle_uri);
    
    // Paneli gösterecek olan:
    httpd_uri_t panel_html = {
        .uri      = "/panel.html",
        .method   = HTTP_GET,
        .handler  = file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &panel_html);    

    ESP_LOGI("WebServer", "Tüm URI handler'ları kaydedildi");
    return ESP_OK;
}
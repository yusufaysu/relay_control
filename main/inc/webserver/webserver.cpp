#include "webserver.hpp"
#include <cJSON.h>

#define TAG "WebServer"
#define LOG_CHUNK_SIZE 200

// Basit oturum kontrolü için cookie adı
#define SESSION_COOKIE_NAME "SESSION_ID"
#define SESSION_COOKIE_VALUE "123456"

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
        // Başarılı girişte cookie ayarla
        httpd_resp_set_hdr(req, "Set-Cookie", SESSION_COOKIE_NAME "=" SESSION_COOKIE_VALUE "; Path=/; HttpOnly");
        httpd_resp_sendstr(req, "OK");
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

esp_err_t WebServer::api_outputs_handler(httpd_req_t *req) {
    // 32 çıkışın durumunu ve ayarlarını JSON olarak hazırla
    const char* resp = "[{\"name\":\"Salon Lamba\",\"type\":\"aydinlatma\",\"state\":true}, {\"name\":\"Mutfak Panjur\",\"type\":\"panjur\",\"state\":\"dur\"}]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::api_inputs_handler(httpd_req_t *req) {
    // 32 girişin durumunu JSON olarak hazırla
    const char* resp = "[{\"active\":true}, {\"active\":false}]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WebServer::api_save_groups_handler(httpd_req_t *req) {
    char buf[2048];
    int len = req->content_len;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
    int received = httpd_req_recv(req, buf, len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Veri alınamadı");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Geçersiz JSON!");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Geçersiz JSON");
        return ESP_FAIL;
    }

    int group_count = cJSON_GetArraySize(root);
    ESP_LOGI(TAG, "Toplam %d grup kaydedildi.", group_count);

    for (int i = 0; i < group_count; ++i) {
        cJSON *group = cJSON_GetArrayItem(root, i);
        cJSON *name = cJSON_GetObjectItem(group, "name");
        cJSON *type = cJSON_GetObjectItem(group, "type");
        cJSON *inputs = cJSON_GetObjectItem(group, "inputs");
        cJSON *outputs = cJSON_GetObjectItem(group, "outputs");
        if (type && strcmp(type->valuestring, "panjur") == 0) {
            if (!inputs || cJSON_GetArraySize(inputs) != 2) {
                cJSON_Delete(root);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Panjurda tam 2 giriş olmalı!");
                return ESP_FAIL;
            }
            if (!outputs || cJSON_GetArraySize(outputs) != 2) {
                cJSON_Delete(root);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Panjurda tam 2 çıkış olmalı!");
                return ESP_FAIL;
            }
        }
        ESP_LOGI(TAG, "Grup %d: %s (%s)", i+1, name ? name->valuestring : "-", type ? type->valuestring : "-");
    }

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// panel.html için özel handler: oturum kontrolü
esp_err_t WebServer::panel_handler(httpd_req_t *req) {
    char cookie[128] = {0};
    size_t cookie_len = httpd_req_get_hdr_value_len(req, "Cookie");
    if (cookie_len > 0 && cookie_len < sizeof(cookie)) {
        httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie));
        if (strstr(cookie, SESSION_COOKIE_NAME "=" SESSION_COOKIE_VALUE)) {
            // Oturum var, paneli göster
            return file_handler(req);
        }
    }
    // Oturum yok, login'e yönlendir
    return redirect_to_login(req);
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
    config.max_uri_handlers = 16;
    config.stack_size = 8192; // veya 12288 de olabilir
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
        .handler  = panel_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &panel_html);

    httpd_uri_t outputs_uri = {
        .uri      = "/api/outputs",
        .method   = HTTP_GET,
        .handler  = api_outputs_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &outputs_uri);

    httpd_uri_t inputs_uri = {
        .uri      = "/api/inputs",
        .method   = HTTP_GET,
        .handler  = api_inputs_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &inputs_uri);

    // Yeni endpoint: /api/save_groups
    httpd_uri_t save_groups_uri = {
        .uri      = "/api/save_groups",
        .method   = HTTP_POST,
        .handler  = api_save_groups_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &save_groups_uri);

    ESP_LOGI("WebServer", "Tüm URI handler'ları kaydedildi");
    return ESP_OK;
}
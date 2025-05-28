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

esp_err_t WebServer::api_save_groups_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Grup kaydetme isteği alındı");
    
    // Buffer boyutunu artırıyoruz
    char *buf = (char*)malloc(req->content_len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Bellek yetersiz! İstenen boyut: %d", req->content_len);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Bellek yetersiz");
        return ESP_FAIL;
    }

    int received = httpd_req_recv(req, buf, req->content_len);
    if (received <= 0) {
        ESP_LOGE(TAG, "Veri alınamadı! Hata kodu: %d", received);
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Veri alınamadı");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    // NVS'ye kaydet
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS açılamadı: %s", esp_err_to_name(err));
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS açılamadı");
        return ESP_FAIL;
    }

    err = nvs_set_blob(nvs_handle, "groups", buf, received);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS'ye yazılamadı: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'ye yazılamadı");
        return ESP_FAIL;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit başarısız: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS commit başarısız");
        return ESP_FAIL;
    }

    nvs_close(nvs_handle);
    free(buf);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServer::api_delete_groups_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Grup silme isteği alındı");

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS açılamadı: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS açılamadı");
        return ESP_FAIL;
    }

    err = nvs_erase_key(nvs_handle, "groups");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "NVS'den silinemedi: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'den silinemedi");
        return ESP_FAIL;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit başarısız: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS commit başarısız");
        return ESP_FAIL;
    }

    nvs_close(nvs_handle);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t WebServer::api_get_groups_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Grup getirme isteği alındı");

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS açılamadı: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS açılamadı");
        return ESP_FAIL;
    }

    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "groups", NULL, &required_size);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // Grup bulunamadı, boş dizi döndür
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "[]");
            nvs_close(nvs_handle);
            return ESP_OK;
        }
        ESP_LOGE(TAG, "NVS'den okunamadı: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'den okunamadı");
        return ESP_FAIL;
    }

    char *buf = (char*)malloc(required_size + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Bellek yetersiz! İstenen boyut: %d", required_size);
        nvs_close(nvs_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Bellek yetersiz");
        return ESP_FAIL;
    }

    err = nvs_get_blob(nvs_handle, "groups", buf, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS'den okunamadı: %s", esp_err_to_name(err));
        free(buf);
        nvs_close(nvs_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'den okunamadı");
        return ESP_FAIL;
    }

    buf[required_size] = '\0';
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);

    free(buf);
    nvs_close(nvs_handle);
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

// Timer task için yapı
struct PanjurTimerParams {
    int timer_value;
    int group_id;
};

// Timer task fonksiyonu
void panjur_timer_task(void *pvParameters) {
    PanjurTimerParams *params = (PanjurTimerParams *)pvParameters;
    
    // Timer süresini bekle
    vTaskDelay(pdMS_TO_TICKS(params->timer_value * 1000));
    
    // Timer süresi dolduğunda panjuru durdur
    ESP_LOGI(TAG, "Timer süresi doldu, panjur durduruluyor (Grup ID: %d)", params->group_id);
    
    // Burada GPIO kontrolü yapılabilir
    
    // Parametreleri temizle
    free(params);
    
    // Task'i sonlandır
    vTaskDelete(NULL);
}

esp_err_t WebServer::api_group_action_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Grup aksiyon isteği alındı");
    
    char buf[100];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) {
        ESP_LOGE(TAG, "Veri alınamadı! Hata kodu: %d", ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Veri alınamadı");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON parse et
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            ESP_LOGE(TAG, "JSON Parse hatası: %s", error_ptr);
        }
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Geçersiz JSON formatı");
        return ESP_FAIL;
    }

    // Gerekli alanları kontrol et
    cJSON *id = cJSON_GetObjectItem(root, "id");
    cJSON *action = cJSON_GetObjectItem(root, "action");
    cJSON *type = cJSON_GetObjectItem(root, "type");

    if (!id || !cJSON_IsNumber(id) || !action || !cJSON_IsString(action) || !type || !cJSON_IsString(type)) {
        ESP_LOGE(TAG, "Eksik veya hatalı parametreler");
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Eksik veya hatalı parametreler");
        return ESP_FAIL;
    }

    // Panjur kontrolü ve timer işlemi
    if (strcmp(type->valuestring, "panjur") == 0) {
        // NVS'den grup bilgilerini al
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS açılamadı: %s", esp_err_to_name(err));
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS açılamadı");
            return ESP_FAIL;
        }

        size_t required_size = 0;
        err = nvs_get_blob(nvs_handle, "groups", NULL, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS'den okunamadı: %s", esp_err_to_name(err));
            nvs_close(nvs_handle);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'den okunamadı");
            return ESP_FAIL;
        }

        char *groups_json = (char*)malloc(required_size + 1);
        if (!groups_json) {
            ESP_LOGE(TAG, "Bellek yetersiz!");
            nvs_close(nvs_handle);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Bellek yetersiz");
            return ESP_FAIL;
        }

        err = nvs_get_blob(nvs_handle, "groups", groups_json, &required_size);
        nvs_close(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS'den okunamadı: %s", esp_err_to_name(err));
            free(groups_json);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS'den okunamadı");
            return ESP_FAIL;
        }
        groups_json[required_size] = '\0';

        // Grupları parse et
        cJSON *groups = cJSON_Parse(groups_json);
        free(groups_json);
        if (!groups) {
            ESP_LOGE(TAG, "Gruplar JSON parse hatası");
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Gruplar JSON parse hatası");
            return ESP_FAIL;
        }

        // İlgili grubu bul
        cJSON *group = NULL;
        cJSON_ArrayForEach(group, groups) {
            cJSON *group_id = cJSON_GetObjectItem(group, "id");
            if (group_id && cJSON_IsNumber(group_id) && (int)group_id->valuedouble == (int)id->valuedouble) {
                break;
            }
        }

        if (!group) {
            ESP_LOGE(TAG, "Grup bulunamadı: %d", (int)id->valuedouble);
            cJSON_Delete(groups);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Grup bulunamadı");
            return ESP_FAIL;
        }

        // Timer değerini al
        cJSON *timer = cJSON_GetObjectItem(group, "timer");
        int timer_value = timer && cJSON_IsNumber(timer) ? (int)timer->valuedouble : 0;

        // Aksiyonu logla
        ESP_LOGI(TAG, "Grup ID: %d, Tip: %s, Aksiyon: %s, Timer: %d saniye", 
                 (int)id->valuedouble, type->valuestring, action->valuestring, timer_value);

        // Timer işlemi için yeni bir task oluştur
        if (strcmp(action->valuestring, "up") == 0 || strcmp(action->valuestring, "down") == 0) {
            // Timer değeri varsa, belirtilen süre sonra durdur
            if (timer_value > 0) {
                // Timer parametrelerini hazırla
                PanjurTimerParams *params = (PanjurTimerParams *)malloc(sizeof(PanjurTimerParams));
                if (params) {
                    params->timer_value = timer_value;
                    params->group_id = (int)id->valuedouble;
                    
                    // Timer için yeni bir task oluştur
                    TaskHandle_t timer_task_handle = NULL;
                    xTaskCreate(
                        panjur_timer_task,
                        "panjur_timer",
                        2048,
                        params,
                        5,
                        &timer_task_handle
                    );
                }
            }
        }

        cJSON_Delete(groups);
    } else {
        // Diğer aksiyonlar için normal log
        ESP_LOGI(TAG, "Grup ID: %d, Tip: %s, Aksiyon: %s", 
                 (int)id->valuedouble, type->valuestring, action->valuestring);
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "OK");
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

    // Yeni endpoint: /api/save_groups
    httpd_uri_t save_groups_uri = {
        .uri      = "/api/save_groups",
        .method   = HTTP_POST,
        .handler  = api_save_groups_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &save_groups_uri);

    // Yeni endpoint: /api/delete_groups
    httpd_uri_t delete_groups_uri = {
        .uri      = "/api/delete_groups",
        .method   = HTTP_POST,
        .handler  = api_delete_groups_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &delete_groups_uri);

    // Yeni endpoint: /api/get_groups
    httpd_uri_t get_groups_uri = {
        .uri      = "/api/get_groups",
        .method   = HTTP_GET,
        .handler  = api_get_groups_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_groups_uri);

    // Yeni endpoint: /api/group_action
    httpd_uri_t group_action_uri = {
        .uri      = "/api/group_action",
        .method   = HTTP_POST,
        .handler  = api_group_action_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &group_action_uri);

    ESP_LOGI("WebServer", "Tüm URI handler'ları kaydedildi");
    return ESP_OK;
}
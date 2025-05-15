#include "inc/mcp23017/mcp23017.hpp"
#include "inc/ethernet/ethernet.hpp"
#include "inc/webserver/webserver.hpp"
#include <dirent.h>
#include <sys/stat.h>

static const char* TAG = "MAIN";

// Global MCP nesneleri
MCP23017 mcp_input_1(0x20);
MCP23017 mcp_input_2(0x22);

// Ethernet nesnesi
Erhernet ethernet;

void log_spiffs_files(const char* path = "/spiffs") {
    ESP_LOGI("SPIFFS", "Listing files in: %s", path);
    
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE("SPIFFS", "Failed to open directory: %s", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char filepath[500];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        filepath[sizeof(filepath) - 1] = '\0';

        struct stat st;
        if (stat(filepath, &st) == 0) {
            ESP_LOGI("SPIFFS", "File: %s (size: %ld bytes)", entry->d_name, st.st_size);
        } else {
            ESP_LOGW("SPIFFS", "Could not stat file: %s", filepath);
        }
    }

    closedir(dir);
}

extern "C" void app_main(void) {
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR servisi kurulamadı! Hata: %s", esp_err_to_name(err));
    }

    mcp_input_1.initInputsWithInterrupt(MCP_INT_GPIO_0X20);
    mcp_input_2.initInputsWithInterrupt(MCP_INT_GPIO_0X22);

    if (!ethernet.begin()) {
        ESP_LOGE(TAG, "Ethernet başlatılamadı!");
        return;
    }

    WebServer server;
    if (server.begin() != ESP_OK)
        ESP_LOGE(TAG, "Web sunucusu başlatılamadı!");

    FILE *f = fopen("/spiffs/login.html", "r");
    if (f) {
        ESP_LOGI("TEST", "login.html exists in SPIFFS!");
        fclose(f);
    } else {
        ESP_LOGE("TEST", "login.html NOT FOUND in SPIFFS!");
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (mcp_input_1.digitalRead(0) == GPIO_LEVEL_LOW) {
            log_spiffs_files();
        }
    }
}
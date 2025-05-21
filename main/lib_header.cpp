#include "lib_header.hpp"

void log_spiffs_files(const char* path) {
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

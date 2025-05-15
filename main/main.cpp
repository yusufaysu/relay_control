#include "inc/mcp23017/mcp23017.hpp"
#include "inc/ethernet/ethernet.hpp"

static const char* TAG = "MAIN";

// Global MCP nesneleri
MCP23017 mcp_input_1(0x20);
MCP23017 mcp_input_2(0x22);

// Ethernet nesnesi
Erhernet ethernet; 

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

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (mcp_input_1.digitalRead(0) == GPIO_LEVEL_LOW) {
            ESP_LOGI(TAG, "------------------");
            mcp_input_1.log_pin_states();
            ESP_LOGI(TAG, "------------------");
            mcp_input_2.log_pin_states();
        }
    }
}
#include "inc/mcp23017/MCPManager.hpp"
#include "inc/ethernet/ethernet.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "dev/light/light.hpp"
#include "esp_log.h"

static const char* HELPER = "HELPER";

// -------------------mcp23017 HELPER FUNCTIONS-------------------

void logMCPPinStates(MCP23017* mcp) {
	for (int i = 0; i < 16; i++) {
		printf("Pin %d: %d\n", i, mcp->pinStates[i]);
	}
}

/*
void gpio_task(void *arg) {
    while (1) {
        int state_10 = gpio_get_level(GPIO_10);
        int state_11 = gpio_get_level(GPIO_11);
        if (state_10 == 0 || state_11 == 0) {
            ESP_LOGI(TAG, "GPIO %d veya GPIO %d LOW seviyesinde!", GPIO_10, GPIO_11);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
*/
    /*
    // GPIO10 & GPIO11 giriş olarak ayarlanıyor
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_10) | (1ULL << GPIO_11),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "GPIO task created.");
    */


/*
    MCP23017* mcp1 = mcpManager.getMCP(0x20);
    if (mcp1) {
        mcp1->pinMode(2, GPIO_MODE_OUTPUT);
        mcp1->digitalWrite(2, 1); // Set the first output to high
        ESP_LOGI(TAG, "MCP 0x20 pin 1 set to output.");
    } else {
        ESP_LOGE(TAG, "Failed to get MCP 0x20.");
    }
*/

/*
    MCP23017* mcp1 = mcpManager.getMCP(0x20);
    MCP23017* mcp2 = mcpManager.getMCP(0x21);
    if (mcp1 && mcp2) {
        mcp1->pinMode(0, GPIO_MODE_OUTPUT);
        mcp2->pinMode(0, GPIO_MODE_INPUT);
        while (1)
        {
            if (!mcp2->digitalRead(0))
            {
                mcp1->digitalWrite(0, 0);
                printf("Buton basıldı\n");
            }
            else
            {
                mcp1->digitalWrite(0, 1);
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        
    }
*/

/*
    // MCP cihazına erişim
    MCP23017* mcp = mcpManager.getMCP(0x23);
    if (mcp) {
        mcp->pinMode(7, GPIO_MODE_OUTPUT);

        // LED yanıp sönme döngüsü
        while (1) {
            mcp->digitalWrite(7, mcp->GPIO_LEVEL_LOW);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            mcp->digitalWrite(7, mcp->GPIO_LEVEL_HIGH);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
*/

/*
Erhernet* ethernet = nullptr;

//main

// Ethernet yöneticisini heap'te oluştur
    ethernet = new Erhernet(SPI_MISO_GPIO_NUM, SPI_MOSI_GPIO_NUM, SPI_SCLK_GPIO_NUM, SPI_CS_GPIO_NUM, SPI_INT_GPIO_NUM);
    
    // Ethernet'i başlat
    if (!ethernet->begin())
    ESP_LOGE(TAG, "Ethernet can not begin.");
*/
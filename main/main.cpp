#include "inc/mcp23017/MCPManager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)13
#define I2C_MASTER_SCL_IO (gpio_num_t)14
#define I2C_MASTER_FREQ_HZ 400000

#define MCP_INT_GPIO GPIO_NUM_10

static const char* TAG = "MAIN";

static QueueHandle_t mcp_int_queue = nullptr;

// ISR sadece pointer gönderir (I2C yapılmaz!)
void IRAM_ATTR gpio_isr_handler(void* arg) {
    MCP23017* mcp = static_cast<MCP23017*>(arg);
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(mcp_int_queue, &mcp, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

// MCP interruptlarını handle eden task
void mcp_interrupt_task(void* arg) {
    MCP23017* mcp;

    // Her pinin son bilinen durumunu tut (16 pin için)
    static bool last_state[16] = {0};

    while (1) {
        if (xQueueReceive(mcp_int_queue, &mcp, portMAX_DELAY)) {
            uint8_t intfA = 0, gpioA = 0;
            uint8_t intfB = 0, gpioB = 0;

            // Değişiklik olmuş pinlerin bilgileri
            mcp->readRegister(0x0E, &intfA);     // INTFA
            mcp->readRegister(0x12, &gpioA);     // GPIOA

            mcp->readRegister(0x0F, &intfB);     // INTFB
            mcp->readRegister(0x13, &gpioB);     // GPIOB

            for (int i = 0; i < 8; i++) {
                if (intfA & (1 << i)) {
                    bool now = (gpioA >> i) & 0x01;
                    if (now != last_state[i]) {
                        ESP_LOGI("MCP_INT", "Port A Pin %d changed: %d -> %d", i, last_state[i], now);
                        last_state[i] = now;
                    }
                }
            }

            for (int i = 0; i < 8; i++) {
                if (intfB & (1 << i)) {
                    bool now = (gpioB >> i) & 0x01;
                    int pin_index = i + 8;
                    if (now != last_state[pin_index]) {
                        ESP_LOGI("MCP_INT", "Port B Pin %d changed: %d -> %d", i, last_state[pin_index], now);
                        last_state[pin_index] = now;
                    }
                }
            }
        }
    }
}

extern "C" void app_main(void) {
    // MCP cihazını ekle
    MCPManager& mcpManager = MCPManager::getInstance();
    mcpManager.addMCP(0x20, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

    MCP23017* mcp = mcpManager.getMCP(0x20);
    if (!mcp) {
        ESP_LOGE(TAG, "MCP init failed.");
        return;
    }

    for (int i = 0; i < 16; i++) {
        mcp->pinMode(i, 1); // input
        mcp->pullUp(i, false);
        mcp->enableInterrupt(i, true); // true: değişim olduğunda interrupt
        ESP_LOGI(TAG, "Pin %d modded as input & interrupt enabled.", i);
    }

    // INT pini ESP32 tarafı
    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << MCP_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // LOW kenarı
    };
    gpio_config(&int_conf);

    // ISR ve kuyruk kurulumu
    mcp_int_queue = xQueueCreate(10, sizeof(MCP23017*));
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(MCP_INT_GPIO, gpio_isr_handler, mcp);
    ESP_LOGI(TAG, "INT handler attached.");

    // Kuyruk dinleyen task
    xTaskCreate(mcp_interrupt_task, "mcp_interrupt_task", 4096, NULL, 10, NULL);

    // Ana döngü boşta bekliyor
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


//EthernetManager* ethernet = nullptr;

// Ethernet yöneticisini heap'te oluştur
// ethernet = new EthernetManager(SPI_MISO_GPIO_NUM, SPI_MOSI_GPIO_NUM, SPI_SCLK_GPIO_NUM, SPI_CS_GPIO_NUM, SPI_INT_GPIO_NUM);

// Ethernet'i başlat
// if (!ethernet->begin())
    // ESP_LOGE(TAG, "Ethernet can not begin.");


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
EthernetManager* ethernet = nullptr;

//main

// Ethernet yöneticisini heap'te oluştur
    ethernet = new EthernetManager(SPI_MISO_GPIO_NUM, SPI_MOSI_GPIO_NUM, SPI_SCLK_GPIO_NUM, SPI_CS_GPIO_NUM, SPI_INT_GPIO_NUM);
    
    // Ethernet'i başlat
    if (!ethernet->begin())
    ESP_LOGE(TAG, "Ethernet can not begin.");
    */
#include "inc/mcp23017/MCPManager.hpp"
#include "inc/ethernet/ethernet_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define ETH_MISO  15
#define ETH_MOSI  16
#define ETH_SCLK  17
#define ETH_CS    5
#define ETH_INT   4
#define ETH_RST   18

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)13
#define I2C_MASTER_SCL_IO (gpio_num_t)14
#define I2C_MASTER_FREQ_HZ 400000

#define MCP_INT_GPIO_20 GPIO_NUM_10
#define MCP_INT_GPIO_22 GPIO_NUM_11

static const char* TAG = "MAIN";

static QueueHandle_t mcp_int_queue = nullptr;
EthernetManager* ethernet = nullptr;

static bool pinStates[2][16] = {0};
static int64_t last_change_time_us[32] = {0};

void IRAM_ATTR gpio_isr_handler(void* arg) {
    MCP23017* mcp = static_cast<MCP23017*>(arg);
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(mcp_int_queue, &mcp, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

void mcp_interrupt_task(void* arg) {
    MCP23017* mcp;
    const int debounce_interval_us = 5000;

    while (1) {
        if (xQueueReceive(mcp_int_queue, &mcp, portMAX_DELAY)) {
            int mcp_index = (mcp->getI2CAddress() == 0x22) ? 1 : 0;
            gpio_num_t int_pin = mcp->getIntGPIO();
            do {
                uint8_t intfA = 0, gpioA = 0;
                uint8_t intfB = 0, gpioB = 0;

                mcp->readRegister(0x0E, &intfA);
                mcp->readRegister(0x12, &gpioA);
                mcp->readRegister(0x0F, &intfB);
                mcp->readRegister(0x13, &gpioB);

                int64_t now_us = esp_timer_get_time();

                for (int i = 0; i < 8; i++) {
                    if (intfA & (1 << i)) {
                        bool now = (gpioA >> i) & 0x01;
                        int pin_index = i;
                        if (now != pinStates[mcp_index][pin_index] &&
                            (now_us - last_change_time_us[mcp_index * 16 + pin_index]) > debounce_interval_us) {
                            ESP_LOGI("MCP_INT", "MCP 0x%02X - Port A Pin %d changed: %d -> %d", mcp->getI2CAddress(), i, pinStates[mcp_index][pin_index], now);
                            pinStates[mcp_index][pin_index] = now;
                            last_change_time_us[mcp_index * 16 + pin_index] = now_us;
                        }
                    }
                }

                for (int i = 0; i < 8; i++) {
                    if (intfB & (1 << i)) {
                        bool now = (gpioB >> i) & 0x01;
                        int pin_index = i + 8;
                        if (now != pinStates[mcp_index][pin_index] &&
                            (now_us - last_change_time_us[mcp_index * 16 + pin_index]) > debounce_interval_us) {
                            ESP_LOGI("MCP_INT", "MCP 0x%02X - Port B Pin %d changed: %d -> %d", mcp->getI2CAddress(), i, pinStates[mcp_index][pin_index], now);
                            pinStates[mcp_index][pin_index] = now;
                            last_change_time_us[mcp_index * 16 + pin_index] = now_us;
                        }
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(2));
            } while (gpio_get_level(int_pin) == 0);
        }
    }
}

void initialize_pin_states(MCP23017* mcp, int mcp_index) {
    uint8_t gpioA = 0, gpioB = 0;
    mcp->readRegister(0x12, &gpioA);
    mcp->readRegister(0x13, &gpioB);

    for (int i = 0; i < 8; i++) {
        pinStates[mcp_index][i] = (gpioA >> i) & 0x01;
        pinStates[mcp_index][i + 8] = (gpioB >> i) & 0x01;
    }
}

extern "C" void app_main(void) {
    MCPManager& mcpManager = MCPManager::getInstance();

    mcpManager.addMCP(0x20, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    mcpManager.addMCP(0x22, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    
    
    MCP23017* mcp_20 = mcpManager.getMCP(0x20);
    MCP23017* mcp_22 = mcpManager.getMCP(0x22);
    
    if (mcp_20) {
        mcp_20->setIntGPIO(MCP_INT_GPIO_20);
        for (int i = 0; i < 16; i++) {
            mcp_20->pinMode(i, 1);
            mcp_20->pullUp(i, true);
            mcp_20->enableInterrupt(i, true);
            ESP_LOGI(TAG, "MCP 0x20 - Pin %d input & interrupt enabled.", i);
        }
        initialize_pin_states(mcp_20, 0);
    } else {
        ESP_LOGE(TAG, "MCP 0x20 init failed.");
        return;
    }

    if (mcp_22) {
        mcp_22->setIntGPIO(MCP_INT_GPIO_22);
        for (int i = 0; i < 16; i++) {
            mcp_22->pinMode(i, 1);
            mcp_22->pullUp(i, true);
            mcp_22->enableInterrupt(i, true);
            ESP_LOGI(TAG, "MCP 0x22 - Pin %d input & interrupt enabled.", i);
        }
        initialize_pin_states(mcp_22, 1);
    } else {
        ESP_LOGE(TAG, "MCP 0x22 init failed.");
        return;
    }

    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << MCP_INT_GPIO_20) | (1ULL << MCP_INT_GPIO_22),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&int_conf);

    mcp_int_queue = xQueueCreate(10, sizeof(MCP23017*));
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(MCP_INT_GPIO_20, gpio_isr_handler, mcp_20);
    gpio_isr_handler_add(MCP_INT_GPIO_22, gpio_isr_handler, mcp_22);
    ESP_LOGI(TAG, "INT handlers attached.");

    xTaskCreate(mcp_interrupt_task, "mcp_interrupt_task", 4096, NULL, 10, NULL);

    // Ethernet yöneticisini heap'te oluştur
    ethernet = new EthernetManager(ETH_MISO, ETH_MOSI, ETH_SCLK, ETH_CS, ETH_INT);

    // Ethernet'i başlat
    if (!ethernet->begin())
        ESP_LOGE(TAG, "Ethernet can not begin.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

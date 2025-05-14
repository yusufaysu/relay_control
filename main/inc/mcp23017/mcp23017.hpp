#pragma once

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <vector>

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   GPIO_NUM_13
#define I2C_MASTER_SCL_IO   GPIO_NUM_14
#define I2C_MASTER_FREQ_HZ  400000

#define MCP_INT_GPIO_0X20 GPIO_NUM_10
#define MCP_INT_GPIO_0X22 GPIO_NUM_11

#define GPIO_LEVEL_LOW  0
#define GPIO_LEVEL_HIGH 1

class MCP23017 {
public:
    MCP23017(uint8_t i2c_address);

    esp_err_t begin();                        // I2C driver kurulum ve cihaz testi
    esp_err_t initInputs(gpio_num_t int_gpio);
    esp_err_t initOutputs();
    esp_err_t pinMode(uint8_t pin, uint8_t mode);
    esp_err_t digitalWrite(uint8_t pin, uint8_t value);
    esp_err_t initInputsWithInterrupt(gpio_num_t int_gpio);
    esp_err_t pullUp(uint8_t pin, bool enable);
    esp_err_t enableInterrupt(uint8_t pin, bool compareToPrevious);
    
    std::vector<uint8_t> getChangedPins();
    
    gpio_num_t getIntGPIO() const;

    uint8_t getRegisterValue(uint8_t reg);
    uint8_t getI2CAddress();
    
    int        digitalRead(uint8_t pin);
    
    void setIntGPIO(gpio_num_t gpio);
    void initialize_pin_states();
    void log_pin_states();
    void attachInterruptHandler(QueueHandle_t queue);
    
private:
    uint8_t _i2c_address;
    gpio_num_t _int_gpio;
    int64_t _lastChangeTimeUs[16] = {0};  // MCP'nin 16 pini için son değişim zamanları
    bool _pinStates[16] = {0};           // Her pinin son bilinen durumu
    static bool _is_driver_installed;
    QueueHandle_t _int_queue = nullptr;

    static void IRAM_ATTR isr_handler(void* arg);
    static void interrupt_task(void* arg);
    
    esp_err_t writeRegister(uint8_t reg, uint8_t value);
    esp_err_t readRegister(uint8_t reg, uint8_t* value);
};

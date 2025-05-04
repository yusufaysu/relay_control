#pragma once

#include "driver/i2c.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include <vector>

#define MCP23017_GPPUA      0x0C  // GPIO Pull-Up Resistor Register for Port A
#define MCP23017_GPPUB      0x0D  // GPIO Pull-Up Resistor Register for Port B
#define MCP23017_IOCON      0x0A  // Ortak konfig register
#define MCP23017_INTFA      0x0E
#define MCP23017_INTFB      0x0F
#define MCP23017_IODIRA     0x00
#define MCP23017_IODIRB     0x01
#define MCP23017_GPINTENA   0x04
#define MCP23017_GPINTENB   0x05
#define MCP23017_DEFVALA    0x06
#define MCP23017_DEFVALB    0x07
#define MCP23017_INTCONA    0x08
#define MCP23017_INTCONB    0x09
#define MCP23017_INTCAPA    0x10
#define MCP23017_INTCAPB    0x11
#define MCP23017_GPIOA      0x12
#define MCP23017_GPIOB      0x13
#define MCP23017_OLATA      0x14
#define MCP23017_OLATB      0x15

class MCP23017 {
    public:
        static const uint8_t GPIO_LEVEL_HIGH = 1;
        static const uint8_t GPIO_LEVEL_LOW = 0;
        bool pinStates[16] = {false};

        MCP23017(i2c_port_t i2c_num, uint8_t i2c_address, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed);
        uint8_t getI2CAddress();
        esp_err_t begin();
        esp_err_t pinMode(uint8_t pin, uint8_t mode);
        esp_err_t digitalWrite(uint8_t pin, uint8_t value);
        int digitalRead(uint8_t pin);
        esp_err_t pullUp(uint8_t pin, bool enable);
        esp_err_t enableInterrupt(uint8_t pin, bool compareToPrevious = true);
        std::vector<uint8_t> getChangedPins();
        esp_err_t readRegister(uint8_t reg, uint8_t *value);
        uint8_t getRegisterValue(uint8_t reg);
        void setIntGPIO(gpio_num_t gpio);
        gpio_num_t getIntGPIO() const;
        
    private:
        i2c_port_t _i2c_num;
        uint8_t _i2c_address;
        gpio_num_t _sda_pin;
        gpio_num_t _scl_pin;
        uint32_t _clk_speed;
        static bool _is_driver_installed;
        gpio_num_t _int_gpio;

        esp_err_t writeRegister(uint8_t reg, uint8_t value);
};

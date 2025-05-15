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
    /**
     * @brief Construct a new MCP23017 object with given I2C address.
     *
     * @param i2c_address I2C address of the MCP23017 device.
     */
    MCP23017(uint8_t i2c_address);

    /**
     * @brief Get the I2C address of the MCP23017 device.
     *
     * @return uint8_t I2C address
     */
    esp_err_t begin();                        // I2C driver kurulum ve cihaz testi
    
    /**
     * @brief Initialize the I2C driver and test communication with the device.
     *
     * Installs I2C driver once and checks whether the device at _i2c_address responds.
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure.
     */
    esp_err_t initInputs(gpio_num_t int_gpio);

    /**
     * @brief Configure the MCP23017 as input with interrupt support.
     *
     * Sets all 16 pins as input, enables pull-ups and interrupts, and initializes pin states.
     *
     * @param int_gpio The ESP32 GPIO used to receive the MCP23017 interrupt.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t initOutputs();

    /**
     * @brief Configure the MCP23017 as output.
     *
     * Sets all 16 pins as output.
     *
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t pinMode(uint8_t pin, uint8_t mode);
    
    /**
     * @brief Set the logic level of a digital output pin.
     *
     * @param pin Pin index (0–15)
     * @param value GPIO_LEVEL_LOW or GPIO_LEVEL_HIGH
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t digitalWrite(uint8_t pin, uint8_t value);

    /**
     * @brief Read the logic level of a digital input pin.
     *
     * @param pin Pin index (0–15)
     * @return int Logic level (0 or 1), or -1 on error.
     */
    esp_err_t initInputsWithInterrupt(gpio_num_t int_gpio);

    /**
     * @brief Enable or disable pull-up resistor for a pin.
     *
     * @param pin Pin index (0–15)
     * @param enable true to enable pull-up, false to disable.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t pullUp(uint8_t pin, bool enable);

    /**
     * @brief Enable interrupt for a pin.
     *
     * @param pin Pin index (0–15)
     * @param compareToPrevious true to compare to previous value, false to compare to default.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t enableInterrupt(uint8_t pin, bool compareToPrevious);
    
    /**
     * @brief Return a list of pin indices that triggered an interrupt.
     *
     * @return std::vector<uint8_t> List of changed pin indices.
     */
    std::vector<uint8_t> getChangedPins();
    
    /**
     * @brief Get the GPIO number configured for interrupt.
     *
     * @return gpio_num_t The interrupt GPIO number.
     */
    gpio_num_t getIntGPIO() const;

    /**
     * @brief Read the current value of a given register.
     *
     * @param reg Register address
     * @return uint8_t Value of the register
     */
    uint8_t getRegisterValue(uint8_t reg);

    /**
     * @brief Get the I2C address of the MCP23017 device.
     *
     * @return uint8_t I2C address
     */
    uint8_t getI2CAddress();
    
    /**
     * @brief Read the logic level of a digital input pin.
     *
     * @param pin Pin index (0–15)
     * @return int Logic level (0 or 1), or -1 on error.
     */
    int        digitalRead(uint8_t pin);
    
    /**
     * @brief Set the GPIO number used for the interrupt signal.
     *
     * @param gpio GPIO number used for INT input.
     */
    void setIntGPIO(gpio_num_t gpio);

    /**
     * @brief Initialize the internal pin state cache by reading GPIOA and GPIOB.
     */
    void initialize_pin_states();

    /**
     * @brief Print the internal pin states to the log.
     */
    void log_pin_states();

    /**
     * @brief Configure the ESP32 interrupt GPIO and register ISR handler for MCP23017.
     *
     * @param queue Queue handle where MCP23017 instances will be posted when an interrupt occurs.
     */
    void attachInterruptHandler(QueueHandle_t queue);
    
private:
    uint8_t _i2c_address;
    gpio_num_t _int_gpio;
    int64_t _lastChangeTimeUs[16] = {0};  // MCP'nin 16 pini için son değişim zamanları
    bool _pinStates[16] = {0};           // Her pinin son bilinen durumu
    static bool _is_driver_installed;
    QueueHandle_t _int_queue = nullptr;

    /**
     * @brief Interrupt Service Routine (ISR) for MCP23017 INT GPIO.
     *
     * @param arg Pointer to the MCP23017 instance.
     */
    static void IRAM_ATTR isr_handler(void* arg);

    /**
     * @brief FreeRTOS task that handles MCP23017 interrupt events.
     *
     * @param arg Pointer to the MCP23017 instance (used to identify the source).
     */
    static void interrupt_task(void* arg);
    
    /**
     * @brief Write a byte to a specific MCP23017 register.
     *
     * @param reg Register address
     * @param value Value to write
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read a byte from a specific MCP23017 register.
     *
     * @param reg Register address
     * @param value Pointer to receive read value
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t readRegister(uint8_t reg, uint8_t* value);
};

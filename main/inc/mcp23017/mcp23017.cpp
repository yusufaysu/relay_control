#include "mcp23017.hpp"

static const char* TAG = "MCP23017";

bool MCP23017::_is_driver_installed = false;

/**
 * @brief Construct a new MCP23017 object with given I2C address.
 *
 * @param i2c_address I2C address of the MCP23017 device.
 */
MCP23017::MCP23017(uint8_t i2c_address) : _i2c_address(i2c_address) {}

/**
 * @brief Get the I2C address of the MCP23017 device.
 *
 * @return uint8_t I2C address
 */
uint8_t MCP23017::getI2CAddress() {
    return _i2c_address;
}

/**
 * @brief Initialize the I2C driver and test communication with the device.
 *
 * Installs I2C driver once and checks whether the device at _i2c_address responds.
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t MCP23017::begin() {
    if (!_is_driver_installed) {
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = I2C_MASTER_SDA_IO;
        conf.scl_io_num = I2C_MASTER_SCL_IO;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
        if (i2c_param_config(I2C_MASTER_NUM, &conf) != ESP_OK) return ESP_FAIL;
        if (i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0) != ESP_OK) return ESP_FAIL;
        _is_driver_installed = true;
    }

    uint8_t test_val;
    if (readRegister(0x00, &test_val) != ESP_OK) {
        ESP_LOGE(TAG, "I2C device not found at 0x%02X", _i2c_address);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Configure the MCP23017 as input with interrupt support.
 *
 * Sets all 16 pins as input, enables pull-ups and interrupts, and initializes pin states.
 *
 * @param int_gpio The ESP32 GPIO used to receive the MCP23017 interrupt.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::initInputs(gpio_num_t int_gpio) {
    setIntGPIO(int_gpio);
    writeRegister(0x00, 0xFF); // IODIRA input
    writeRegister(0x01, 0xFF); // IODIRB input

    for (int i = 0; i < 16; i++) {
        pullUp(i, true);
        enableInterrupt(i, true);
    }

    initialize_pin_states();
    return ESP_OK;
}

/**
 * @brief Configure the MCP23017 as output.
 *
 * Sets all 16 pins as output.
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::initOutputs() {
    writeRegister(0x00, 0x00); // IODIRA output
    writeRegister(0x01, 0x00); // IODIRB output
    return ESP_OK;
}

/**
 * @brief Configure a single pin as input or output.
 *
 * @param pin Pin index (0–15)
 * @param mode GPIO_MODE_INPUT or GPIO_MODE_OUTPUT
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::pinMode(uint8_t pin, uint8_t mode) {
    uint8_t reg = (pin < 8) ? 0x00 : 0x01;
    uint8_t bit = pin % 8;
    uint8_t value;
    if (readRegister(reg, &value) != ESP_OK) return ESP_FAIL;
    if (mode == GPIO_MODE_OUTPUT) value &= ~(1 << bit);
    else value |= (1 << bit);
    return writeRegister(reg, value);
}

/**
 * @brief Set the logic level of a digital output pin.
 *
 * @param pin Pin index (0–15)
 * @param value GPIO_LEVEL_LOW or GPIO_LEVEL_HIGH
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::digitalWrite(uint8_t pin, uint8_t value) {
    uint8_t reg = (pin < 8) ? 0x14 : 0x15;
    uint8_t bit = pin % 8;
    uint8_t reg_value;
    if (readRegister(reg, &reg_value) != ESP_OK) return ESP_FAIL;
    if (value == GPIO_LEVEL_HIGH) reg_value |= (1 << bit);
    else reg_value &= ~(1 << bit);
    _pinStates[pin] = value;
    return writeRegister(reg, reg_value);
}

/**
 * @brief Read the logic level of a digital input pin.
 *
 * @param pin Pin index (0–15)
 * @return int Logic level (0 or 1), or -1 on error.
 */
int MCP23017::digitalRead(uint8_t pin) {
    uint8_t reg = (pin < 8) ? 0x12 : 0x13;
    uint8_t bit = pin % 8;
    uint8_t value;
    if (readRegister(reg, &value) != ESP_OK) return -1;
    _pinStates[pin] = (value >> bit) & 1;
    return _pinStates[pin];
}

/**
 * @brief Enable or disable pull-up resistor for a pin.
 *
 * @param pin Pin index (0–15)
 * @param enable true to enable pull-up, false to disable.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::pullUp(uint8_t pin, bool enable) {
    uint8_t reg = (pin < 8) ? 0x0C : 0x0D;
    uint8_t bit = pin % 8;
    uint8_t value;
    if (readRegister(reg, &value) != ESP_OK) return ESP_FAIL;
    if (enable) value |= (1 << bit);
    else value &= ~(1 << bit);
    return writeRegister(reg, value);
}

/**
 * @brief Enable interrupt for a pin.
 *
 * @param pin Pin index (0–15)
 * @param compareToPrevious true to compare to previous value, false to compare to default.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t MCP23017::enableInterrupt(uint8_t pin, bool compareToPrevious) {
    uint8_t port = (pin < 8) ? 0 : 1;
    uint8_t bit = pin % 8;

    uint8_t gpinten, intcon;
    readRegister(port == 0 ? 0x04 : 0x05, &gpinten);
    gpinten |= (1 << bit);
    writeRegister(port == 0 ? 0x04 : 0x05, gpinten);

    readRegister(port == 0 ? 0x08 : 0x09, &intcon);
    if (compareToPrevious) intcon &= ~(1 << bit);
    else intcon |= (1 << bit);
    return writeRegister(port == 0 ? 0x08 : 0x09, intcon);
}

/**
 * @brief Return a list of pin indices that triggered an interrupt.
 *
 * @return std::vector<uint8_t> List of changed pin indices.
 */
std::vector<uint8_t> MCP23017::getChangedPins() {
    std::vector<uint8_t> result;
    uint8_t a, b;
    if (readRegister(0x0E, &a) == ESP_OK) {
        for (int i = 0; i < 8; i++) if (a & (1 << i)) result.push_back(i);
    }
    if (readRegister(0x0F, &b) == ESP_OK) {
        for (int i = 0; i < 8; i++) if (b & (1 << i)) result.push_back(i + 8);
    }
    return result;
}

/**
 * @brief Read the current value of a given register.
 *
 * @param reg Register address
 * @return uint8_t Value of the register
 */
uint8_t MCP23017::getRegisterValue(uint8_t reg) {
    uint8_t val = 0;
    readRegister(reg, &val);
    return val;
}

/**
 * @brief Set the GPIO number used for the interrupt signal.
 *
 * @param gpio GPIO number used for INT input.
 */
void MCP23017::setIntGPIO(gpio_num_t gpio) { _int_gpio = gpio; }

/**
 * @brief Get the GPIO number configured for interrupt.
 *
 * @return gpio_num_t The interrupt GPIO number.
 */
gpio_num_t MCP23017::getIntGPIO() const { return _int_gpio; }

/**
 * @brief Initialize the internal pin state cache by reading GPIOA and GPIOB.
 */
void MCP23017::initialize_pin_states(){
    uint8_t gpioA = 0, gpioB = 0;
    this->readRegister(0x12, &gpioA);
    this->readRegister(0x13, &gpioB);

    for (int i = 0; i < 8; i++) {
        _pinStates[i] = (gpioA >> i) & 0x01;
        _pinStates[i + 8] = (gpioB >> i) & 0x01;
        _lastChangeTimeUs[i] = esp_timer_get_time();
        _lastChangeTimeUs[i + 8] = esp_timer_get_time();
    }
}

/**
 * @brief Print the internal pin states to the log.
 */
void MCP23017::log_pin_states() {
    for (int i = 0; i < 16; i++) {
        ESP_LOGI(TAG, "0x%02X Pin %2d = %d", _i2c_address, i, _pinStates[i]);
    }
}

/**
 * @brief Write a byte to a specific MCP23017 register.
 *
 * @param reg Register address
 * @param value Value to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t MCP23017::writeRegister(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Read a byte from a specific MCP23017 register.
 *
 * @param reg Register address
 * @param value Pointer to receive read value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t MCP23017::readRegister(uint8_t reg, uint8_t* value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Configure the ESP32 interrupt GPIO and register ISR handler for MCP23017.
 *
 * Sets the given GPIO as input with pull-up and interrupt on falling edge.
 * Registers the MCP23017 instance as argument for the ISR and assigns it to a shared queue.
 *
 * @param queue Queue handle where MCP23017 instances will be posted when an interrupt occurs.
 */
void MCP23017::attachInterruptHandler(QueueHandle_t queue) {
    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << _int_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&int_conf);

    gpio_isr_handler_add(_int_gpio, MCP23017::isr_handler, this);  // ISR fonksiyonu dışarıdan gelecek
    ESP_LOGI("MCP", "INT pin %d configured for MCP 0x%02X", _int_gpio, _i2c_address);
}

/**
 * @brief Interrupt Service Routine (ISR) for MCP23017 INT GPIO.
 *
 * This static function is triggered when the MCP23017 interrupt pin goes LOW.
 * It posts the current MCP23017 instance to the queue from ISR context.
 *
 * @param arg Pointer to the MCP23017 instance.
 */
void IRAM_ATTR MCP23017::isr_handler(void* arg) {
    MCP23017* mcp = static_cast<MCP23017*>(arg);
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(mcp->_int_queue, &mcp, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

/**
 * @brief FreeRTOS task that handles MCP23017 interrupt events.
 *
 * This task continuously waits for MCP23017 instances posted from ISR.
 * When triggered, it reads the INTF and INTCAP registers to detect which pins changed,
 * and logs their new states with debounce filtering.
 *
 * @param arg Pointer to the MCP23017 instance (used to identify the source).
 */
void MCP23017::interrupt_task(void* arg) {
    MCP23017* mcp = static_cast<MCP23017*>(arg);
    const int debounce_interval_us = 5000;

    while (true) {
        MCP23017* received_mcp = nullptr;
        if (xQueueReceive(mcp->_int_queue, &received_mcp, portMAX_DELAY)) {
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
                    bool now = (gpioA >> i) & 1;
                    if ((intfA & (1 << i)) && now != mcp->_pinStates[i] &&
                        (now_us - mcp->_lastChangeTimeUs[i]) > debounce_interval_us) {
                        ESP_LOGI("MCP_ISR", "0x%02X A%d: %d -> %d", mcp->_i2c_address, i, mcp->_pinStates[i], now);
                        mcp->_pinStates[i] = now;
                        mcp->_lastChangeTimeUs[i] = now_us;
                    }
                }

                for (int i = 0; i < 8; i++) {
                    bool now = (gpioB >> i) & 1;
                    if ((intfB & (1 << i)) && now != mcp->_pinStates[i] &&
                        (now_us - mcp->_lastChangeTimeUs[i]) > debounce_interval_us) {
                        ESP_LOGI("MCP_ISR", "0x%02X A%d: %d -> %d", mcp->_i2c_address, i, mcp->_pinStates[i], now);
                        mcp->_pinStates[i] = now;
                        mcp->_lastChangeTimeUs[i] = now_us;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(2));
            } while (gpio_get_level(int_pin) == 0);
        }
    }
}

/**
 * @brief Initialize MCP23017 for input use with interrupt handling.
 *
 * Configures all pins as input with pull-ups and enables interrupts.
 * Also sets up the ESP32 interrupt GPIO, creates a task, and connects the ISR and queue.
 *
 * @param int_gpio ESP32 GPIO pin connected to the MCP23017 INT output.
 * @return esp_err_t ESP_OK on success, or error code if setup fails.
 */
esp_err_t MCP23017::initInputsWithInterrupt(gpio_num_t int_gpio) {
    setIntGPIO(int_gpio);

    esp_err_t ret = begin();
    if (ret != ESP_OK) return ret;

    // IODIR ayarı
    writeRegister(0x00, 0xFF);
    writeRegister(0x01, 0xFF);

    for (int i = 0; i < 16; i++) {
        pullUp(i, true);
        enableInterrupt(i, true);
    }

    initialize_pin_states();

    // INT pin GPIO ayarı
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << int_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&cfg);

    gpio_isr_handler_add(int_gpio, isr_handler, this);

    // Kuyruk ve task oluştur
    _int_queue = xQueueCreate(10, sizeof(MCP23017*));
    xTaskCreate(interrupt_task, "mcp_int_task", 4096, this, 10, nullptr);

    ESP_LOGI("MCP_INIT", "MCP 0x%02X giriş ve INT sistemi hazır.", _i2c_address);
    return ESP_OK;
}
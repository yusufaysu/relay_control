#include "mcp23017.hpp"

// I2C driver'ın durumunu takip eden statik bir değişken ekleyelim.
bool MCP23017::_is_driver_installed = false;

MCP23017::MCP23017(i2c_port_t i2c_num, uint8_t i2c_address, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed) 
    : _i2c_num(i2c_num), _i2c_address(i2c_address), _sda_pin(sda_pin), _scl_pin(scl_pin), _clk_speed(clk_speed) {}

uint8_t MCP23017::getI2CAddress() {
    return _i2c_address;
}

esp_err_t MCP23017::begin() {
    esp_err_t ret;

    // I2C driver sadece bir kez kurulur.
    if (!_is_driver_installed) {
        ESP_LOGI("MCP23017", "Configuring I2C...");

        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = _sda_pin;
        conf.scl_io_num = _scl_pin;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.clk_flags = 0;
        conf.master.clk_speed = _clk_speed;

        ret = i2c_param_config(_i2c_num, &conf);
        if (ret != ESP_OK) {
            ESP_LOGE("MCP23017", "I2C parameter configuration failed");
            return ret;
        }

        ret = i2c_driver_install(_i2c_num, conf.mode, 0, 0, 0);
        if (ret != ESP_OK) {
            ESP_LOGE("MCP23017", "I2C driver installation failed");
            return ret;
        }

        ESP_LOGI("MCP23017", "Configured I2C successfully");
        _is_driver_installed = true; // Driver kuruldu olarak işaretlenir.

        // I2C cihazlarını tarayalım.
        ESP_LOGI("MCP23017", "Scanning I2C bus...");
        for (uint8_t i = 1; i < 127; i++) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            ret = i2c_master_cmd_begin(_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                ESP_LOGI("MCP23017", "Found device at address 0x%02X", i);
            }
        }
    } else {
        ESP_LOGI("MCP23017", "I2C driver already installed, skipping configuration.");
    }

    // MCP23017 ile iletişimi kontrol et.
    uint8_t test_value;
    ret = readRegister(MCP23017_IODIRA, &test_value);
    if (ret != ESP_OK) {
        ESP_LOGE("MCP23017", "Failed to communicate with device at address 0x%02x", _i2c_address);
        return ESP_FAIL;
    }

    ESP_LOGI("MCP23017", "Successfully communicated with device");

    // MCP23017 yapılandırması
    ret = writeRegister(MCP23017_IODIRA, 0xFF); // Set all A pins as input
    if (ret != ESP_OK) {
        ESP_LOGE("MCP23017", "Failed to configure IODIRA register");
        return ret;
    }
    
    ret = writeRegister(MCP23017_IODIRB, 0xFF); // Set all B pins as input
    if (ret != ESP_OK) {
        ESP_LOGE("MCP23017", "Failed to configure IODIRB register");
        return ret;
    }

    return ESP_OK;
}

esp_err_t MCP23017::pinMode(uint8_t pin, uint8_t mode) {
    // Set pin mode for a specific pin
    uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
    uint8_t bit = (pin < 8) ? pin : pin - 8;
    uint8_t value;
    esp_err_t ret = readRegister(reg, &value);
    if (ret != ESP_OK) return ret;
    if (mode == GPIO_MODE_OUTPUT) {
        value &= ~(1 << bit);
    } else {
        value |= (1 << bit);
    }
    return writeRegister(reg, value);
}

esp_err_t MCP23017::digitalWrite(uint8_t pin, uint8_t value) {
    // Write digital value to a specific pin
    uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
    uint8_t bit = (pin < 8) ? pin : pin - 8;
    uint8_t reg_value;
    esp_err_t ret = readRegister(reg, &reg_value);
    if (ret != ESP_OK) return ret;
    if (value == GPIO_LEVEL_HIGH) {
        reg_value |= (1 << bit);
        pinStates[pin] = true; // Pin durumunu güncelle
    } else {
        reg_value &= ~(1 << bit);
        pinStates[pin] = false; // Pin durumunu güncelle
    }
    return writeRegister(reg, reg_value);
}

int MCP23017::digitalRead(uint8_t pin) {
    // Read digital value from a specific pin
    uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    uint8_t bit = (pin < 8) ? pin : pin - 8;
    uint8_t value;
    esp_err_t ret = readRegister(reg, &value);
    if (ret != ESP_OK) return -1;
    bool pinState = (value & (1 << bit)) ? true : false;
    pinStates[pin] = pinState; // Pin durumunu güncelle
    
    return pinState ? 1 : 0;
}

esp_err_t MCP23017::writeRegister(uint8_t reg, uint8_t value) {
    // Write a byte to a specific register
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t MCP23017::readRegister(uint8_t reg, uint8_t *value) {
    // Read a byte from a specific register
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t MCP23017::pullUp(uint8_t pin, bool enable) {
    uint8_t reg = (pin < 8) ? MCP23017_GPPUA : MCP23017_GPPUB;
    uint8_t bit = (pin < 8) ? pin : pin - 8;
    uint8_t value;

    esp_err_t ret = readRegister(reg, &value);
    if (ret != ESP_OK) return ret;

    if (enable)
        value |= (1 << bit);
    else
        value &= ~(1 << bit);

    return writeRegister(reg, value);
}

esp_err_t MCP23017::enableInterrupt(uint8_t pin, bool compareToPrevious) {
    uint8_t port = (pin < 8) ? 0 : 1;
    uint8_t bit = (pin < 8) ? pin : pin - 8;

    uint8_t gpinten = 0, intcon = 0;

    // Enable interrupt on pin
    readRegister(port == 0 ? MCP23017_GPINTENA : MCP23017_GPINTENB, &gpinten);
    gpinten |= (1 << bit);
    writeRegister(port == 0 ? MCP23017_GPINTENA : MCP23017_GPINTENB, gpinten);

    // INTCON: 0 = previous value (change), 1 = compare to DEFVAL
    readRegister(port == 0 ? MCP23017_INTCONA : MCP23017_INTCONB, &intcon);
    if (compareToPrevious)
        intcon &= ~(1 << bit);  // Change = interrupt
    else
        intcon |= (1 << bit);   // Compare to DEFVAL
    return writeRegister(port == 0 ? MCP23017_INTCONA : MCP23017_INTCONB, intcon);
}

std::vector<uint8_t> MCP23017::getChangedPins() {
    std::vector<uint8_t> changedPins;
    uint8_t intfA = 0, intfB = 0;

    if (readRegister(MCP23017_INTFA, &intfA) == ESP_OK) {
        for (uint8_t i = 0; i < 8; i++) {
            if (intfA & (1 << i)) changedPins.push_back(i);  // A portu pin i
        }
    }

    if (readRegister(MCP23017_INTFB, &intfB) == ESP_OK) {
        for (uint8_t i = 0; i < 8; i++) {
            if (intfB & (1 << i)) changedPins.push_back(i + 8);  // B portu pin i+8
        }
    }

    return changedPins;
}

uint8_t MCP23017::getRegisterValue(uint8_t reg) {
    uint8_t value = 0;
    if (readRegister(reg, &value) != ESP_OK) {
        ESP_LOGE("MCP23017", "Failed to read register 0x%02X", reg);
    }
    return value;
}
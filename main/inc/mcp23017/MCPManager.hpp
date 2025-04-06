#pragma once

#include <map>
#include "mcp23017.hpp"
#include "esp_log.h"

class MCPManager {
public:
    static MCPManager& getInstance(); // Singleton instansı

    bool addMCP(uint8_t address, i2c_port_t i2c_num, gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz);
    MCP23017* getMCP(uint8_t address);
    void cleanup(); // Tüm kaynakları temizler

private:
    MCPManager() = default;  // Constructor
    ~MCPManager() = default; // Destructor
    MCPManager(const MCPManager&) = delete;
    MCPManager& operator=(const MCPManager&) = delete;

    std::map<uint8_t, MCP23017*> mcp_map; 
};

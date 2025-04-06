#include "MCPManager.hpp"

static const char* TAG = "MCPManager";

MCPManager& MCPManager::getInstance() {
    static MCPManager instance;
    return instance;
}

bool MCPManager::addMCP(uint8_t address, i2c_port_t i2c_num, gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz) {
    if (mcp_map.find(address) != mcp_map.end()) {
        ESP_LOGW(TAG, "MCP23017 at address 0x%02X already exists!", address);
        return false;
    }

    MCP23017* mcp = new MCP23017(i2c_num, address, sda, scl, freq_hz);
    if (mcp->begin() == ESP_OK) {
        mcp_map[address] = mcp;
        ESP_LOGI(TAG, "MCP23017 added at address 0x%02X", address);
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize MCP23017 at address 0x%02X", address);
        delete mcp;
        return false;
    }
}

MCP23017* MCPManager::getMCP(uint8_t address) {
    auto it = mcp_map.find(address);
    if (it != mcp_map.end()) {
        return it->second;
    }
    ESP_LOGE(TAG, "MCP23017 at address 0x%02X not found!", address);
    return nullptr;
}

void MCPManager::cleanup() {
    for (auto& pair : mcp_map) {
        delete pair.second;
    }
    mcp_map.clear();
    ESP_LOGI(TAG, "All MCP23017 instances cleaned up.");
}

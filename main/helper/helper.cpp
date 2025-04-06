#include "inc/mcp23017/MCPManager.hpp"
#include "inc/ethernet/ethernet_manager.hpp"
#include "dev/light/light.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void logMCPPinStates(MCP23017* mcp) {
	for (int i = 0; i < 16; i++) {
		printf("Pin %d: %d\n", i, mcp->pinStates[i]);
	}
}
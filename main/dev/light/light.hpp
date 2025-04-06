#pragma once

#include "inc/mcp23017/MCPManager.hpp"
#include <vector>

struct PinPair {
    uint8_t inputPin;
    uint8_t outputPin;
    bool lastInputState = false;
    bool lastOutputState = false;
};

class Light {
    public:
        Light(MCP23017* inputMCP, MCP23017* outputMCP);

        void addInputOutputPair(uint8_t inputPin, uint8_t outputPin);
        void update();

    private:
        MCP23017* inputMCP;
        MCP23017* outputMCP;
        std::vector<PinPair> pinPairs;

};

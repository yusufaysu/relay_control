#include "light.hpp"

static const char* TAG = "LIGHT";

Light::Light(MCP23017* inputMCP, MCP23017* outputMCP)
    : inputMCP(inputMCP), outputMCP(outputMCP) {}

void Light::addInputOutputPair(uint8_t inputPin, uint8_t outputPin) {
    pinPairs.push_back({inputPin, outputPin, true, true});

    // Set the input pin as input and enable pull-up resistor
    inputMCP->pinMode(inputPin, GPIO_MODE_INPUT);

    // Set the output pin as output
    outputMCP->pinMode(outputPin, GPIO_MODE_OUTPUT);
    outputMCP->digitalWrite(outputPin, 1); // Default to off (high state for relay off)
}

void Light::update() {
    for (auto& pair : pinPairs) {
        bool inputState = inputMCP->digitalRead(pair.inputPin);

        if (pair.lastInputState != inputState) {
            ESP_LOGI(TAG, "Input state changed: %d", inputState);
            pair.lastInputState = inputState;
        }

        bool newOutputState = !inputState; // Active low input
        if (pair.lastOutputState != newOutputState) {
            outputMCP->digitalWrite(pair.outputPin, newOutputState ? 0 : 1);
            pair.lastOutputState = newOutputState;
        }
    }
}
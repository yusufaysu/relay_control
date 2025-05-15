#!/bin/bash

# Build the project
echo "⏳ Building project..."
idf.py build
if [ $? -ne 0 ]; then
    echo "❌ Build failed. Exiting..."
    exit 1
fi

# Flash firmware
echo "⏳ Flashing firmware..."
idf.py flash
if [ $? -ne 0 ]; then
    echo "❌ Flash failed. Exiting..."
    exit 1
fi

# Create SPIFFS image
echo "⏳ Creating SPIFFS image..."
idf.py spiffs-create
if [ $? -ne 0 ]; then
    echo "❌ SPIFFS creation failed. Exiting..."
    exit 1
fi

# Flash SPIFFS image
echo "⏳ Flashing SPIFFS image to 0x290000..."
esptool.py --port /dev/tty.usbmodem112301 write_flash 0x290000 build/spiffs.bin
if [ $? -ne 0 ]; then
    echo "❌ SPIFFS flash failed. Exiting..."
    exit 1
fi

# Start serial monitor
echo "✅ All steps successful. Starting monitor..."
idf.py monitor
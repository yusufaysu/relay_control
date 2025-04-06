#pragma once

#include <stdio.h>
#include <string>
#include <chrono>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_ping.h"
#include "driver/spi_master.h"
#include "esp_eth_enc28j60.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "ping/ping_sock.h"
#include "nvs_flash.h"
#include "nvs.h"

#define HOST (spi_host_device_t) 1
#define CLOCK_MHZ 8

class EthernetManager {
public:
    bool eth_ready = false;

    EthernetManager(int miso_pin, int mosi_pin, int sclk_pin, int cs_pin, int int_pin);
    ~EthernetManager();
    
    bool begin();
    void startPing(const std::string& hostname, uint32_t interval_ms = 10000);
    void stopPing();
    void logNetworkInfo();

private:
    static void ethEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void gotIpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void pingTask(void* pvParameters);

    struct PingTaskParams {
        std::string hostname;
        uint32_t interval_ms;
    };

    int miso_pin_;
    int mosi_pin_;
    int sclk_pin_;
    int cs_pin_;
    int int_pin_;
    
    esp_netif_t* eth_netif_;
    esp_eth_handle_t eth_handle_ = NULL;
    TaskHandle_t ping_task_handle_;
    PingTaskParams ping_params_;

    bool getMacAddress(uint8_t* mac_addr);
}; 
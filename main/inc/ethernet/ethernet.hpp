// ethernet.hpp
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

#define ETH_MISO  15
#define ETH_MOSI  16
#define ETH_SCLK  17
#define ETH_CS    5
#define ETH_INT   4
#define ETH_RST   18
#define HOST (spi_host_device_t) 1
#define CLOCK_MHZ 8

class Erhernet {
public:
    /**
     * @brief Flag indicating whether the Ethernet interface has obtained an IP address.
     */
    bool eth_ready = false;

    /**
     * @brief Default constructor.
     * Initializes internal state; no hardware interaction.
     */
    Erhernet();

    /**
     * @brief Destructor.
     * Stops any running ping task and uninstalls the Ethernet driver.
     */
    ~Erhernet();

    /**
     * @brief Initialize and start the Ethernet interface.
     *
     * Configures the interrupt pin, initializes network interface and event loop,
     * registers event handlers, sets up the SPI bus for ENC28J60,
     * installs the Ethernet driver, sets the MAC address, attaches to the network interface,
     * and starts the driver.
     *
     * @return true if initialization succeeds, false otherwise.
     */
    bool begin();

    /**
     * @brief Start periodic ICMP pinging of the specified hostname.
     *
     * Stops any existing ping task, stores the hostname and interval,
     * and spawns a FreeRTOS task to perform continuous pings.
     *
     * @param hostname Hostname or IP address to ping.
     * @param interval_ms Milliseconds between pings (default: 10000 ms).
     */
    void startPing(const std::string& hostname, uint32_t interval_ms = 10000);

    /**
     * @brief Stop the running ping task.
     *
     * Deletes the FreeRTOS ping task if active and clears its handle.
     */
    void stopPing();

    /**
     * @brief Log the current network configuration.
     *
     * Retrieves IP address, netmask, gateway, and DNS servers
     * for the Ethernet interface and logs them.
     */
    void logNetworkInfo();

    /**
     * @brief Delete the stored MAC address from NVS storage.
     *
     * Removes the "eth_mac" key so that a fresh MAC is generated on next initialization.
     *
     * @return true if deletion succeeds or key is not present, false on error.
     */
    bool deleteMacAddressFromNvs();

private:
    /**
     * @brief Handle Ethernet driver events.
     *
     * Logs connection, disconnection, start, and stop events.
     *
     * @param arg Pointer to the network interface handle.
     * @param event_base Base of the event (ETH_EVENT).
     * @param event_id Specific Ethernet event ID.
     * @param event_data Event data (unused).
     */
    static void ethEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    /**
     * @brief Handle IP acquisition event.
     *
     * Called when the Ethernet interface obtains an IP. Sets eth_ready flag
     * to true and logs network info.
     *
     * @param arg Pointer to the Erhernet instance.
     * @param event_base Base of the event (IP_EVENT).
     * @param event_id Specific IP event ID.
     * @param event_data Event data (unused).
     */
    static void gotIpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    /**
     * @brief FreeRTOS task function to perform ICMP ping operations.
     *
     * Resolves the hostname to an IP, configures ping parameters and callbacks,
     * starts the ping session, and blocks until stopped.
     *
     * @param pvParameters Pointer to PingTaskParams containing settings.
     */
    static void pingTask(void* pvParameters);

    /**
     * @brief Structure holding parameters for the ping task.
     */
    struct PingTaskParams {
        std::string hostname;   /**< Hostname or IP to ping. */
        uint32_t interval_ms;   /**< Interval between pings in ms. */
    };

    /**
     * @brief Retrieve or generate a MAC address.
     *
     * Attempts to read a saved MAC from NVS. If not found, generates a new MAC
     * using the EFUSE base MAC and esp_random(), then saves it to NVS.
     *
     * @param mac_addr Pointer to a 6-byte array to store the MAC.
     * @return true on success, false on error.
     */
    bool getMacAddress(uint8_t* mac_addr);

    esp_netif_t* eth_netif_;              /**< Network interface handle. */
    esp_eth_handle_t eth_handle_ = NULL;  /**< Ethernet driver handle. */
    TaskHandle_t ping_task_handle_;       /**< Handle for the ping task. */
    PingTaskParams ping_params_;          /**< Parameters for ping task. */
};
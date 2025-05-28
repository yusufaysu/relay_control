// ethernet.cpp
#include "ethernet.hpp"

static const char* TAG = "ETHERNET";

/**
 * @brief Default constructor for Erhernet.
 *
 * Initializes internal state without interacting with hardware.
 */
Erhernet::Erhernet() {}

/**
 * @brief Destructor for Erhernet.
 *
 * Stops any ongoing ping task and uninstalls the Ethernet driver if it was installed.
 */
Erhernet::~Erhernet() {
    stopPing();
    if (eth_handle_) {
        esp_eth_stop(eth_handle_);
        esp_eth_driver_uninstall(eth_handle_);
    }
}

/**
 * @brief Retrieve or generate a MAC address for Ethernet.
 *
 * Initializes NVS, attempts to read a stored MAC address. If unavailable,
 * generates a new MAC using esp_random() and EFUSE base MAC, saves it to NVS.
 *
 * @param mac_addr Buffer to receive the MAC address (6 bytes).
 * @return true on success.
 * @return false on failure.
 */
bool Erhernet::getMacAddress(uint8_t* mac_addr) {
    bool mac_from_nvs = false;

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = 6;
        err = nvs_get_blob(nvs_handle, "eth_mac", mac_addr, &required_size);
        if (err == ESP_OK && required_size == 6) {
            mac_from_nvs = true;
            ESP_LOGI(TAG, "MAC address read from NVS");
        }
    }

    if (!mac_from_nvs) {
        // Generate new MAC
        uint8_t base_mac_addr[6];
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(base_mac_addr));
        mac_addr[0] = (uint8_t)(esp_random() & 0xFC);
        mac_addr[1] = (uint8_t)(esp_random() & 0xFF);
        mac_addr[2] = (uint8_t)(esp_random() & 0xFF);
        mac_addr[3] = base_mac_addr[3];
        mac_addr[4] = base_mac_addr[4];
        mac_addr[5] = base_mac_addr[5];

        // Save new MAC to NVS
        err = nvs_set_blob(nvs_handle, "eth_mac", mac_addr, 6);
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "New MAC address saved to NVS");
            }
        }
    }

    nvs_close(nvs_handle);
    return true;
}

/**
 * @brief Delete the stored MAC address from NVS.
 *
 * Removes the "eth_mac" key so that next initialization generates a fresh MAC.
 *
 * @return true if deletion succeeds or key not found, false on error.
 */
bool Erhernet::deleteMacAddressFromNvs() {
    // Initialize NVS if not already
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS storage: %s", esp_err_to_name(err));
        return false;
    }

    // Erase the eth_mac key
    err = nvs_erase_key(nvs_handle, "eth_mac");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase MAC from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS after erase: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "MAC address deleted from NVS");
    return true;
}

/**
 * @brief Set up and start the Ethernet interface.
 *
 * Configures GPIO for interrupt, initializes network interface and event loop,
 * registers callbacks for Ethernet and IP events, sets up SPI bus and ENC28J60 driver,
 * assigns MAC address, attaches and starts the Ethernet driver.
 *
 * @return true if successful, false otherwise.
 */
bool Erhernet::begin() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << ETH_INT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif_ = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &gotIpEventHandler, this));

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = ETH_MISO;
    buscfg.mosi_io_num = ETH_MOSI;
    buscfg.sclk_io_num = ETH_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 1600;
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = CLOCK_MHZ * 1000000;
    spi_devcfg.spics_io_num = ETH_CS;
    spi_devcfg.queue_size = 20;
    spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CLOCK_MHZ);

    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = ETH_INT;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t* mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 5000;
    phy_config.reset_gpio_num = -1;
    phy_config.phy_addr = 0;
    esp_eth_phy_t* phy = esp_eth_phy_new_enc28j60(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle_));

    uint8_t custom_mac_addr[6];
    if (!getMacAddress(custom_mac_addr)) {
        ESP_LOGE(TAG, "Failed to get MAC address");
        return false;
    }
    ESP_ERROR_CHECK(mac->set_addr(mac, custom_mac_addr));
    ESP_LOGI(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", 
             custom_mac_addr[0], custom_mac_addr[1], custom_mac_addr[2], 
             custom_mac_addr[3], custom_mac_addr[4], custom_mac_addr[5]);

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif_, esp_eth_new_netif_glue(eth_handle_)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle_));

    return true;
}

/**
 * @brief Start the ICMP ping task.
 *
 * Stops any existing ping task, saves parameters, and creates a new FreeRTOS task.
 *
 * @param hostname Target host to ping.
 * @param interval_ms Interval between pings in milliseconds.
 */
void Erhernet::startPing(const std::string& hostname, uint32_t interval_ms) {
    stopPing();
    ping_params_.hostname = hostname;
    ping_params_.interval_ms = interval_ms;
    xTaskCreate(pingTask, "ping_task", 4096, &ping_params_, 5, &ping_task_handle_);
}

/**
 * @brief Stop the ICMP ping task.
 *
 * Deletes the FreeRTOS ping task if it exists.
 */
void Erhernet::stopPing() {
    if (ping_task_handle_) {
        vTaskDelete(ping_task_handle_);
        ping_task_handle_ = nullptr;
    }
}

/**
 * @brief Callback for successful ping.
 *
 * Logs bytes received, remote IP, sequence number, TTL, and round-trip time.
 */
static void test_on_ping_success(esp_ping_handle_t hdl, void* args) {
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI("PING", "%ld bytes from %s icmp_seq=%d ttl=%d time=%ld ms",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

/**
 * @brief Callback for ping timeout.
 *
 * Logs a warning when a ping sequence times out.
 */
static void test_on_ping_timeout(esp_ping_handle_t hdl, void* args) {
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW("PING", "From %s icmp_seq=%d timeout", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

/**
 * @brief Callback when ping session ends.
 *
 * Logs a summary of packets transmitted, received, and total time.
 */
static void test_on_ping_end(esp_ping_handle_t hdl, void* args) {
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    ESP_LOGI("PING", "%ld packets transmitted, %ld received, time %ldms", 
             transmitted, received, total_time_ms);
}

/**
 * @brief FreeRTOS task to perform continuous ping operations.
 *
 * Resolves hostname to IP, configures ping session, registers callbacks,
 * starts ping, and blocks until notified to stop.
 */
void Erhernet::pingTask(void* pvParameters) {
    auto* params = static_cast<PingTaskParams*>(pvParameters);
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo* res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    getaddrinfo(params->hostname.c_str(), NULL, &hint, &res);
    if (res == NULL) {
        ESP_LOGE("PING", "Hostname çözümlenemedi: %s", params->hostname.c_str());
        vTaskDelete(NULL);
        return;
    }
    struct in_addr addr4 = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = ESP_PING_COUNT_INFINITE;
    ping_config.interval_ms = params->interval_ms;

    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    cbs.cb_args = NULL;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

/**
 * @brief Handle Ethernet driver events such as connect/disconnect.
 */
void Erhernet::ethEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    static uint32_t reconnect_attempts = 0;
    const uint32_t MAX_RECONNECT_ATTEMPTS = 5;
    const uint32_t RECONNECT_DELAY_MS = 2000;
    auto* eth = static_cast<Erhernet*>(arg);

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet connected");
            reconnect_attempts = 0;
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet disconnected");
            if (reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
                reconnect_attempts++;
                ESP_LOGI(TAG, "Yeniden bağlanma denemesi %lu/%lu", reconnect_attempts, MAX_RECONNECT_ATTEMPTS);
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
                if (eth->eth_handle_) {
                    esp_eth_stop(eth->eth_handle_);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_eth_start(eth->eth_handle_);
                }
            } else {
                ESP_LOGE(TAG, "Maksimum yeniden bağlanma denemesi aşıldı");
                reconnect_attempts = 0;
            }
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            break;
    }
}

/**
 * @brief Handle IP_EVENT_ETH_GOT_IP event.
 *
 * Marks eth_ready true and logs network information.
 */
void Erhernet::gotIpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ESP_LOGI(TAG, "IP obtained");
        auto* eth = static_cast<Erhernet*>(arg);
        eth->eth_ready = true;
        eth->logNetworkInfo();
    }
}

/**
 * @brief Log Ethernet interface IP, netmask, gateway, and DNS settings.
 */
void Erhernet::logNetworkInfo() {
    if (!eth_netif_) {
        ESP_LOGE(TAG, "Ethernet interface not initialized");
        return;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif_, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "Failed to get IP information");
    }

    esp_netif_dns_info_t dns_info;
    for (int i = 0; i < ESP_NETIF_DNS_MAX; i++) {
        if (esp_netif_get_dns_info(eth_netif_, (esp_netif_dns_type_t)i, &dns_info) == ESP_OK) {
            if (dns_info.ip.u_addr.ip4.addr != 0) {
                ESP_LOGI(TAG, "DNS[%d]: " IPSTR, i, IP2STR(&dns_info.ip.u_addr.ip4));
            }
        }
    }
}
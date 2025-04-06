#include "ethernet_manager.hpp"

static const char* TAG = "ETH_MANAGER";

EthernetManager::EthernetManager(int miso_pin, int mosi_pin, int sclk_pin, 
                               int cs_pin, int int_pin)
    : miso_pin_(miso_pin), mosi_pin_(mosi_pin), sclk_pin_(sclk_pin),
      cs_pin_(cs_pin), int_pin_(int_pin), eth_netif_(nullptr),
      eth_handle_(nullptr), ping_task_handle_(nullptr) {
}

EthernetManager::~EthernetManager() {
    stopPing();
    if (eth_handle_) {
        esp_eth_stop(eth_handle_);
        esp_eth_driver_uninstall(eth_handle_);
    }
}

bool EthernetManager::getMacAddress(uint8_t* mac_addr) {
    bool mac_from_nvs = false;

    // NVS'yi başlat
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // NVS handle aç
    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = 6;
        err = nvs_get_blob(nvs_handle, "eth_mac", mac_addr, &required_size);
        
        if (err == ESP_OK && required_size == 6) {
            mac_from_nvs = true;
            ESP_LOGI(TAG, "MAC adresi NVS'den okundu");
        }
    }

    if (!mac_from_nvs) {
        // MAC adresi NVS'de yoksa yeni oluştur
        uint8_t base_mac_addr[6];
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(base_mac_addr));
        
        mac_addr[0] = (uint8_t)(esp_random() & 0xFC);
        mac_addr[1] = (uint8_t)(esp_random() & 0xFF);
        mac_addr[2] = (uint8_t)(esp_random() & 0xFF);
        mac_addr[3] = base_mac_addr[3];
        mac_addr[4] = base_mac_addr[4];
        mac_addr[5] = base_mac_addr[5];

        // Yeni MAC adresini NVS'ye kaydet
        err = nvs_set_blob(nvs_handle, "eth_mac", mac_addr, 6);
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Yeni MAC adresi NVS'ye kaydedildi");
            }
        }
    }

    nvs_close(nvs_handle);
    return true;
}

bool EthernetManager::begin() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << int_pin_);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif_ = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethEventHandler, this->eth_netif_));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &gotIpEventHandler, this));

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = miso_pin_;
    buscfg.mosi_io_num = mosi_pin_;
    buscfg.sclk_io_num = sclk_pin_;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4000;
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = CLOCK_MHZ * 1000 * 1000;
    spi_devcfg.spics_io_num = cs_pin_;
    spi_devcfg.queue_size = 20;
    spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CLOCK_MHZ);

    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = int_pin_;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    this->eth_handle_ = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle_));

    uint8_t custom_mac_addr[6];
    if (!getMacAddress(custom_mac_addr)) {
        ESP_LOGE(TAG, "MAC adresi alınamadı");
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

// 2 parametre alır: hostname ve ping aralığı (ms) ve bir task oluşturur
// Bu task, belirtilen süre aralığında belirtilen hostname'e ping atar
// ping başarılı olduğunda test_on_ping_success fonksiyonunu çağırır
// ping başarısız olduğunda test_on_ping_timeout fonksiyonunu çağırır
// ping işlemi sona erdiğinde test_on_ping_end fonksiyonunu çağırır
void EthernetManager::startPing(const std::string& hostname, uint32_t interval_ms) {
    stopPing();
    ping_params_.hostname = hostname;
    ping_params_.interval_ms = interval_ms;
    xTaskCreate(pingTask, "ping_task", 4096, &ping_params_, 5, &ping_task_handle_);
}

void EthernetManager::stopPing() {
    if (ping_task_handle_) {
        vTaskDelete(ping_task_handle_);
        ping_task_handle_ = nullptr;
    }
}

static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
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

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW("PING", "From %s icmp_seq=%d timeout", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    ESP_LOGI("PING", "%ld packets transmitted, %ld received, time %ldms", 
             transmitted, received, total_time_ms);
}

void EthernetManager::pingTask(void* pvParameters) {
    auto* params = static_cast<PingTaskParams*>(pvParameters);
    
    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    getaddrinfo((char*)params->hostname.c_str(), NULL, &hint, &res);
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;          // target IP address
    ping_config.count = ESP_PING_COUNT_INFINITE;    // ping in infinite mode, esp_ping_stop can stop it
    ping_config.interval_ms = params->interval_ms;  // ping interval in ms

    /* set callback functions */
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

void EthernetManager::ethEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet bağlı");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet bağlantısı kesildi");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet başlatıldı");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet durduruldu");
            break;
    }
}

void EthernetManager::gotIpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    auto* eth = static_cast<EthernetManager*>(arg);
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ESP_LOGI(TAG, "IP alındı");
        eth->eth_ready = true;
        //eth->startPing("google.com", 2 * 1000);
        eth->logNetworkInfo();
    }
}

void EthernetManager::logNetworkInfo() {
    if (!eth_netif_) {
        ESP_LOGE(TAG, "Ethernet interface handle alınamadı.");
        return;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif_, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "IP bilgisi alınamadı.");
    }

    esp_netif_dns_info_t dns_info;
    for (int i = 0; i < ESP_NETIF_DNS_MAX; i++) {
        if (esp_netif_get_dns_info(eth_netif_, (esp_netif_dns_type_t) i, &dns_info) == ESP_OK) {
            if (dns_info.ip.u_addr.ip4.addr != 0) {
                ESP_LOGI(TAG, "DNS[%d]: " IPSTR, i, IP2STR(&dns_info.ip.u_addr.ip4));
            }
        }
    }
}
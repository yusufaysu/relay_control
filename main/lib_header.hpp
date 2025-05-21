#pragma once

#include <stdio.h>
#include <string>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
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

void log_spiffs_files(const char* path = "/spiffs");
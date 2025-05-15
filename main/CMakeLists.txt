idf_component_register(
    SRCS 
        "main.cpp"
        "inc/ethernet/ethernet.cpp"
        "inc/mcp23017/mcp23017.cpp"
        "inc/webserver/webserver.cpp"
    INCLUDE_DIRS 
        "."
        "inc"
    REQUIRES 
        driver 
        esp_eth 
        esp_netif 
        lwip 
        espressif__enc28j60
        nvs_flash
        i2c_bus
        esp_timer
        esp_http_server
        spiffs
)

# SPIFFS içeriğini yükle
spiffs_create_partition_image(spiffs ../spiffs FLASH_IN_PROJECT)
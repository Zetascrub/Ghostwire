idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "." "../../shared/protocol"
                       REQUIRES driver esp_driver_tsens esp_eth esp_event
                                esp_http_server esp_netif esp_timer json
                                mbedtls mdns nvs_flash)

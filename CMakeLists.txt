idf_component_register(SRCS "app_main.c"
                    PRIV_REQUIRES mqtt nvs_flash esp_netif
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_client driver)

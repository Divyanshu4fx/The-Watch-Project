idf_component_register(SRCS "smart_watch.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd esp_adc bt nvs_flash)

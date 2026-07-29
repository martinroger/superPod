idf_component_register(SRCS "src/esPod.cpp"
                            "src/L0x00.cpp"
                            "src/L0x03.cpp"
                            "src/L0x04.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_ringbuf esp_driver_uart esp_driver_gpio
                       PRIV_REQUIRES driver freertos esp_timer log)

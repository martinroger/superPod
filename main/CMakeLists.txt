idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES pl2303_usb espod bt_a2dp_sink driver esp_driver_uart esp_driver_gpio esp_timer freertos log)

idf_component_register(SRCS "src/pl2303_usb.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_tinyusb esp_driver_uart esp_driver_gpio
                       PRIV_REQUIRES driver freertos log)

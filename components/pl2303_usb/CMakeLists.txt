idf_component_register(SRCS "src/pl2303_usb.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_tinyusb esp_driver_uart esp_driver_gpio
                       PRIV_REQUIRES driver freertos log)

if(CONFIG_PL2303_USB_LOG_LEVEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DLOG_LOCAL_LEVEL=${CONFIG_PL2303_USB_LOG_LEVEL}")
endif()

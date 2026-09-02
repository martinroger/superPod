idf_component_register(SRCS "src/ble_audio_sink.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES bt esp_driver_i2s esp_driver_gpio nvs_flash freertos log)


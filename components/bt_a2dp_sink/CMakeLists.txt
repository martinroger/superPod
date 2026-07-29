idf_component_register(SRCS "src/bt_a2dp_sink.cpp" "src/i2s_audio.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES bt esp_driver_i2s
                       PRIV_REQUIRES nvs_flash driver freertos log)

# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_compile_options(-w) # Turn off warnings until esp_tinyusb is updated IEC-86

# Non-invasive global compatibility definitions for external components under ESP-IDF v6.2 (master)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/main/audio_tools_compat.h>)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD OFF)

project(superPod)

# Non-invasively attach esp_driver_i2s dependency to external component INTERFACE targets
if(TARGET __idf_pschatzmann__arduino-audio-tools)
    target_link_libraries(__idf_pschatzmann__arduino-audio-tools INTERFACE __idf_esp_driver_i2s)
    target_include_directories(__idf_pschatzmann__arduino-audio-tools INTERFACE $ENV{IDF_PATH}/components/esp_driver_i2s/include)
endif()

if(TARGET __idf_pschatzmann__ESP32-A2DP)
    target_link_libraries(__idf_pschatzmann__ESP32-A2DP INTERFACE __idf_esp_driver_i2s)
    target_include_directories(__idf_pschatzmann__ESP32-A2DP INTERFACE $ENV{IDF_PATH}/components/esp_driver_i2s/include)
endif()

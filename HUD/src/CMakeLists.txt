# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c ${CMAKE_SOURCE_DIR}/src/*.cpp)

idf_component_register(
    SRCS ${app_sources}
    PRIV_REQUIRES
        bt
        mbedtls
        nvs_flash
        esp_http_server
        esp_wifi
)
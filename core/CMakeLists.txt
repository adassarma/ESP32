set(SOURCES ./Src/main.cpp)

idf_component_register(SRCS ${SOURCES}
                            INCLUDE_DIRS . Inc
                            REQUIRES freertos nvs_flash esp_wifi vfs driver)
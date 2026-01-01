idf_component_register(
    SRCS "app_main.cpp" "app_tasks.cpp" "matter_airq.cpp"
    INCLUDE_DIRS "."
    REQUIRES rrh62000 nvs_flash esp_matter freertos driver button
)
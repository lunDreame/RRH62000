#include "app_tasks.hpp"
#include "matter_airq.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_matter.h"

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = matter_airq::init();
    if (ret != ESP_OK) {
        ESP_LOGE("app_main", "Failed to initialize Matter: %s", esp_err_to_name(ret));
        return;
    }

    ret = sensor_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE("app_main", "Failed to start sensor task: %s", esp_err_to_name(ret));
        return;
    }

    ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE("app_main", "Failed to initialize button: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("app_main", "Application started");
}
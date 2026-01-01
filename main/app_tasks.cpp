#include "app_tasks.hpp"
#include "app_config.hpp"
#include "matter_airq.hpp"
#include "rrh62000/rrh62000.hpp"
#include "rrh62000/rrh62000_uart_transport.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "iot_button.h"
#include "button_gpio.h"

namespace {
constexpr const char* TAG = "app_tasks";
button_handle_t button_handle = nullptr;

void button_long_press_cb(void* button_handle, void* usr_data) {
    ESP_LOGI(TAG, "Button long press detected - triggering factory reset");
    matter_airq::factory_reset();
}
}

static void sensor_task(void* pvParameters) {
    rrh62000::UartTransport transport(app_config::UART_NUM, app_config::UART_TX_PIN, app_config::UART_RX_PIN);
    esp_err_t ret = transport.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UART transport: %s", esp_err_to_name(ret));
        vTaskDelete(nullptr);
        return;
    }

    rrh62000::Rrh62000 sensor(&transport);
    ret = sensor.set_passive_mode();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set passive mode: %s", esp_err_to_name(ret));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Sensor task started");

    rrh62000::Reading reading;
    while (true) {
        ret = sensor.read_passive(reading, 1000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %f°C, Humidity: %f%%, PM1.0: %f, PM2.5: %f, PM10: %f, CO2: %f, TVOC: %f, IAQ: %f",
                     reading.temperature_c, reading.humidity_rh, reading.pm1_0_mass, reading.pm2_5_mass, reading.pm10_mass, reading.eco2, reading.tvoc, reading.iaq);

            ret = matter_airq::publish(reading);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to publish to Matter: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "Failed to read sensor: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(app_config::SENSOR_READ_INTERVAL_MS));
    }
}

esp_err_t sensor_task_start() {
    BaseType_t ret = xTaskCreate(sensor_task, "sensor_task", 4096, nullptr, 5, nullptr);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t button_init() {
    button_config_t button_cfg = {
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = 0
    };

    button_gpio_config_t gpio_cfg = {
        .gpio_num = app_config::BUTTON_GPIO,
        .active_level = 0,  // Active low (BOOT button is typically active low)
        .enable_power_save = false,
        .disable_pull = false
    };

    esp_err_t ret = iot_button_new_gpio_device(&button_cfg, &gpio_cfg, &button_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button: %s", esp_err_to_name(ret));
        return ret;
    }

    button_event_args_t long_press_args = {};
    long_press_args.long_press.press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS;
    ret = iot_button_register_cb(button_handle, BUTTON_LONG_PRESS_START, &long_press_args, button_long_press_cb, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callback: %s", esp_err_to_name(ret));
        iot_button_delete(button_handle);
        button_handle = nullptr;
        return ret;
    }

    ESP_LOGI(TAG, "Button initialized on GPIO %d (long press %d ms for factory reset)", 
             app_config::BUTTON_GPIO, CONFIG_BUTTON_LONG_PRESS_TIME_MS);
    return ESP_OK;
}
#include "app_tasks.hpp"
#include "app_config.hpp"
#include "matter_airq.hpp"
#include "rrh62000/rrh62000.hpp"
#include "rrh62000/rrh62000_uart_transport.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {
constexpr const char* TAG = "app_tasks";
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
            ESP_LOGI(TAG, "T=%.2f°C, RH=%.2f%%, PM2.5=%.1f, PM10=%.1f, CO2=%.0f, TVOC=%.1f, IAQ=%.2f",
                     reading.temperature_c, reading.humidity_rh, reading.pm2_5_mass, reading.pm10_mass,
                     reading.eco2, reading.tvoc, reading.iaq);

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
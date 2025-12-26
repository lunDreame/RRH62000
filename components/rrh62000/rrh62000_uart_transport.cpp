#include "rrh62000/rrh62000_uart_transport.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr const char* TAG = "rrh62000_uart";
constexpr uint32_t UART_BUF_SIZE = 1024;
constexpr uint8_t START_BYTE_1 = 0xFF;
constexpr uint8_t START_BYTE_2 = 0xFA;
}

namespace rrh62000 {

UartTransport::UartTransport(uart_port_t uart_num, int tx_pin, int rx_pin)
    : uart_num_(uart_num), tx_pin_(tx_pin), rx_pin_(rx_pin), initialized_(false) {
}

UartTransport::~UartTransport() {
    if (initialized_) {
        uart_driver_delete(uart_num_);
    }
}

esp_err_t UartTransport::init() {
    uart_config_t uart_config = {};
    uart_config.baud_rate = 9600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t ret = uart_param_config(uart_num_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(uart_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(uart_num_, UART_BUF_SIZE, UART_BUF_SIZE, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "UART%d initialized: TX=%d, RX=%d", uart_num_, tx_pin_, rx_pin_);
    return ESP_OK;
}

esp_err_t UartTransport::read(uint8_t* data, size_t len, uint32_t timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    int bytes_read = uart_read_bytes(uart_num_, data, len, pdMS_TO_TICKS(timeout_ms));
    if (bytes_read < 0) {
        return ESP_FAIL;
    }
    if (static_cast<size_t>(bytes_read) < len) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t UartTransport::write(const uint8_t* data, size_t len) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    int bytes_written = uart_write_bytes(uart_num_, data, len);
    if (bytes_written < 0 || static_cast<size_t>(bytes_written) != len) {
        return ESP_FAIL;
    }

    esp_err_t ret = uart_wait_tx_done(uart_num_, pdMS_TO_TICKS(100));
    return ret;
}

esp_err_t UartTransport::read_frame_sync(uint8_t* frame, size_t frame_len, uint32_t timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start_ticks) < timeout_ticks) {
        size_t bytes_available = 0;
        uart_get_buffered_data_len(uart_num_, &bytes_available);

        if (bytes_available < 2) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint8_t byte1, byte2;
        int read1 = uart_read_bytes(uart_num_, &byte1, 1, 0);
        if (read1 != 1) {
            continue;
        }

        if (byte1 == START_BYTE_1) {
            int read2 = uart_read_bytes(uart_num_, &byte2, 1, pdMS_TO_TICKS(100));
            if (read2 == 1 && byte2 == START_BYTE_2) {
                frame[0] = START_BYTE_1;
                frame[1] = START_BYTE_2;

                int remaining = uart_read_bytes(uart_num_, frame + 2, frame_len - 2, pdMS_TO_TICKS(500));
                if (remaining == static_cast<int>(frame_len - 2)) {
                    return ESP_OK;
                }
            }
        }
    }

    return ESP_ERR_TIMEOUT;
}

} // namespace rrh62000
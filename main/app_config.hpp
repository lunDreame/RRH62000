#pragma once

#include "driver/uart.h"

namespace app_config {

constexpr uart_port_t UART_NUM = UART_NUM_2;
constexpr int UART_TX_PIN = 17;
constexpr int UART_RX_PIN = 16;
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 3000;

// Button configuration
constexpr int BUTTON_GPIO = 0;  // GPIO 0 (BOOT button on ESP32S)

} // namespace app_config
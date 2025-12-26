#pragma once

#include "driver/uart.h"

namespace app_config {

constexpr uart_port_t UART_NUM = UART_NUM_2;
constexpr int UART_TX_PIN = 17;
constexpr int UART_RX_PIN = 16;
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 3000;

} // namespace app_config
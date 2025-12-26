#pragma once

#include "rrh62000_transport.hpp"
#include "driver/uart.h"
#include "esp_err.h"

namespace rrh62000 {

class UartTransport : public Transport {
public:
    UartTransport(uart_port_t uart_num, int tx_pin, int rx_pin);
    ~UartTransport() override;

    esp_err_t init();
    esp_err_t read(uint8_t* data, size_t len, uint32_t timeout_ms) override;
    esp_err_t write(const uint8_t* data, size_t len) override;
    esp_err_t read_frame_sync(uint8_t* frame, size_t frame_len, uint32_t timeout_ms) override;

private:
    uart_port_t uart_num_;
    int tx_pin_;
    int rx_pin_;
    bool initialized_;
};

} // namespace rrh62000
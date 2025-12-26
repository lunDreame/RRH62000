#pragma once

#include "rrh62000_types.hpp"
#include "rrh62000_transport.hpp"
#include "esp_err.h"

namespace rrh62000 {

class Rrh62000 {
public:
    explicit Rrh62000(Transport* transport);
    ~Rrh62000() = default;

    esp_err_t set_passive_mode();
    esp_err_t read_passive(Reading& reading, uint32_t timeout_ms = 1000);

private:
    esp_err_t parse_uart_frame(const uint8_t* frame, Reading& reading);
    Transport* transport_;
};

} // namespace rrh62000
#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace rrh62000 {

class Transport {
public:
    virtual ~Transport() = default;
    virtual esp_err_t read(uint8_t* data, size_t len, uint32_t timeout_ms) = 0;
    virtual esp_err_t write(const uint8_t* data, size_t len) = 0;
    virtual esp_err_t read_frame_sync(uint8_t* frame, size_t frame_len, uint32_t timeout_ms) = 0;
};

} // namespace rrh62000
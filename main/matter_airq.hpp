#pragma once

#include "rrh62000/rrh62000_types.hpp"
#include "esp_err.h"

namespace matter_airq {

esp_err_t init();
esp_err_t publish(const rrh62000::Reading& reading);
esp_err_t factory_reset();

} // namespace matter_airq
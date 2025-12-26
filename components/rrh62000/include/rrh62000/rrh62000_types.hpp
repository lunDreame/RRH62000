#pragma once

#include <cstdint>

namespace rrh62000 {

struct Reading {
    float temperature_c;      // °C
    float humidity_rh;        // %RH
    float pm1_0_mass;         // µg/m³
    float pm2_5_mass;         // µg/m³
    float pm10_mass;          // µg/m³
    float pm0_5_count;        // /cm³
    float pm1_0_count;        // /cm³
    float pm2_5_count;        // /cm³
    float pm5_0_count;        // /cm³
    float pm10_count;         // /cm³
    float tvoc;               // µg/m³
    float eco2;               // ppm
    float iaq;                // IAQ index
    bool fan_error;
    bool fan_speed_error;
};

} // namespace rrh62000
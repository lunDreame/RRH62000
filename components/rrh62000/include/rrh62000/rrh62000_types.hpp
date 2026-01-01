#pragma once

#include <cstdint>

namespace rrh62000 {

struct Reading {
    float temperature_c;      // °C
    float humidity_rh;        // %RH
    
    // Number Concentration (/cm³)
    float pm0_3_count;        // 0.3µm number concentration
    float pm0_5_count;        // 0.5µm number concentration
    float pm1_0_count;        // 1.0µm number concentration
    float pm2_5_count;        // 2.5µm number concentration
    float pm5_0_count;        // 4.0µm number concentration (closest to 5.0µm)
    
    // Mass Concentration Set 1 (KCI particle reference) (µg/m³)
    float pm1_0_mass;         // PM1.0 Set 1
    float pm2_5_mass;         // PM2.5 Set 1
    float pm10_mass;          // PM10 Set 1
    
    // Mass Concentration Set 2 (cigarette smoke reference) (µg/m³)
    float pm1_0_mass_2;      // PM1.0 Set 2
    float pm2_5_mass_2;      // PM2.5 Set 2
    float pm10_mass_2;       // PM10 Set 2
    
    // Gas / IAQ
    float tvoc;               // µg/m³
    float eco2;               // ppm
    float iaq;                // IAQ index
    uint16_t relative_iaq;    // Relative IAQ (Reserved)
    
    // Status flags
    bool fan_error;
    bool fan_speed_error;
};

} // namespace rrh62000
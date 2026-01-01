#include "rrh62000/rrh62000.hpp"
#include "esp_log.h"
#include <cstring>

namespace {
constexpr const char* TAG = "rrh62000";
constexpr uint8_t CMD_START_BYTE_1 = 0xA1;
constexpr uint8_t CMD_START_BYTE_2 = 0x4D;
constexpr size_t CMD_FRAME_LENGTH = 7;
constexpr uint8_t CMD_PASSIVE_MODE = 0xE1;
constexpr uint8_t CMD_READ = 0xE2;
constexpr uint8_t DATA_FRAME_START_1 = 0xFF;
constexpr uint8_t DATA_FRAME_START_2 = 0xFA;
constexpr size_t DATA_FRAME_LENGTH = 39;
}

namespace rrh62000 {

Rrh62000::Rrh62000(Transport* transport) : transport_(transport) {
}

static void build_command_frame(uint8_t* frame, uint8_t cmd, uint8_t datah, uint8_t datal) {
    frame[0] = CMD_START_BYTE_1;
    frame[1] = CMD_START_BYTE_2;
    frame[2] = cmd;
    frame[3] = datah;
    frame[4] = datal;
    
    uint16_t checksum = CMD_START_BYTE_1 + CMD_START_BYTE_2 + cmd + datah + datal;
    frame[5] = (checksum >> 8) & 0xFF;
    frame[6] = checksum & 0xFF;
}

esp_err_t Rrh62000::set_passive_mode() {
    uint8_t cmd_frame[CMD_FRAME_LENGTH];
    build_command_frame(cmd_frame, CMD_PASSIVE_MODE, 0x00, 0x00);
    return transport_->write(cmd_frame, CMD_FRAME_LENGTH);
}

esp_err_t Rrh62000::read_passive(Reading& reading, uint32_t timeout_ms) {
    uint8_t cmd_frame[CMD_FRAME_LENGTH];
    build_command_frame(cmd_frame, CMD_READ, 0x00, 0x00);
    
    esp_err_t ret = transport_->write(cmd_frame, CMD_FRAME_LENGTH);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t data_frame[DATA_FRAME_LENGTH];
    ret = transport_->read_frame_sync(data_frame, DATA_FRAME_LENGTH, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    return parse_uart_frame(data_frame, reading);
}

esp_err_t Rrh62000::parse_uart_frame(const uint8_t* frame, Reading& reading) {
    if (frame[0] != DATA_FRAME_START_1 || frame[1] != DATA_FRAME_START_2) {
        ESP_LOGE(TAG, "Invalid data frame start bytes: 0x%02X 0x%02X", frame[0], frame[1]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t checksum = 0;
    for (size_t i = 0; i < DATA_FRAME_LENGTH - 1; ++i) {
        checksum += frame[i];
    }
    checksum %= 256;

    if (checksum != frame[DATA_FRAME_LENGTH - 1]) {
        ESP_LOGE(TAG, "Data frame checksum mismatch: calculated=0x%02X, received=0x%02X", checksum, frame[DATA_FRAME_LENGTH - 1]);
        return ESP_ERR_INVALID_CRC;
    }

    auto read_be16 = [](const uint8_t* p) -> int16_t {
        return (static_cast<int16_t>(p[0]) << 8) | p[1];
    };

    auto read_be16u = [](const uint8_t* p) -> uint16_t {
        return (static_cast<uint16_t>(p[0]) << 8) | p[1];
    };

    // Parse status word
    uint16_t status = read_be16u(&frame[2]);
    // Status bits
    // Bit 0: High Concentration flag
    // Bit 1: Dust Accumulation flag
    // Bit 2: Fan Speed Error flag
    // Bit 3: Fan Error flag
    reading.fan_error = (status & (1 << 3)) != 0;
    reading.fan_speed_error = (status & (1 << 2)) != 0;

    // Number Concentration (/cm³)
    reading.pm0_3_count = read_be16u(&frame[4]) * 0.1f;   // NC_0.3 (0.3µm)
    reading.pm0_5_count = read_be16u(&frame[6]) * 0.1f;   // NC_0.5 (0.5µm)
    reading.pm1_0_count = read_be16u(&frame[8]) * 0.1f;   // NC_1 (1.0µm)
    reading.pm2_5_count = read_be16u(&frame[10]) * 0.1f;  // NC_2.5 (2.5µm)
    reading.pm5_0_count = read_be16u(&frame[12]) * 0.1f;  // NC_4 (4.0µm, closest available to 5.0µm)

    // Mass Concentration Set 1 (KCI particle reference) (µg/m³)
    reading.pm1_0_mass = read_be16u(&frame[14]) * 0.1f;  // PM1_1
    reading.pm2_5_mass = read_be16u(&frame[16]) * 0.1f;  // PM2.5_1
    reading.pm10_mass = read_be16u(&frame[18]) * 0.1f;   // PM10_1

    // Mass Concentration Set 2 (cigarette smoke reference) (µg/m³)
    reading.pm1_0_mass_2 = read_be16u(&frame[20]) * 0.1f;  // PM1_2
    reading.pm2_5_mass_2 = read_be16u(&frame[22]) * 0.1f;  // PM2.5_2
    reading.pm10_mass_2 = read_be16u(&frame[24]) * 0.1f;   // PM10_2

    // Environment
    reading.temperature_c = read_be16(&frame[26]) * 0.01f;  // Temperature (°C)
    reading.humidity_rh = read_be16u(&frame[28]) * 0.01f;   // Relative Humidity (%RH)

    // Gas / IAQ
    reading.tvoc = read_be16u(&frame[30]) * 10.0f;  // TVOC (µg/m³)
    reading.eco2 = read_be16u(&frame[32]) * 1.0f;   // eCO2 (ppm)
    reading.iaq = read_be16u(&frame[34]) * 0.01f;   // IAQ index
    reading.relative_iaq = read_be16u(&frame[36]);  // Relative IAQ (Reserved)

    //ESP_LOGI(TAG, "Temperature: %f°C, Humidity: %f%%, PM0.3: %f, PM0.5: %f, PM1.0: %f, PM2.5: %f, PM5.0: %f, PM10: %f, PM1.0 Mass: %f, PM2.5 Mass: %f, PM10 Mass: %f, PM1.0 Mass 2: %f, PM2.5 Mass 2: %f, PM10 Mass 2: %f, TVOC: %f, eCO2: %f, IAQ: %f, Relative IAQ: %d", reading.temperature_c, reading.humidity_rh, reading.pm0_3_count, reading.pm0_5_count, reading.pm1_0_count, reading.pm2_5_count, reading.pm5_0_count, 0.0f, reading.pm1_0_mass, reading.pm2_5_mass, reading.pm10_mass, reading.pm1_0_mass_2, reading.pm2_5_mass_2, reading.pm10_mass_2, reading.tvoc, reading.eco2, reading.iaq, reading.relative_iaq);

    return ESP_OK;
}

} // namespace rrh62000
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

    reading.temperature_c = read_be16(&frame[2]) * 0.01f;
    reading.humidity_rh = read_be16u(&frame[4]) * 0.01f;
    reading.pm1_0_mass = read_be16u(&frame[6]) * 0.1f;
    reading.pm2_5_mass = read_be16u(&frame[8]) * 0.1f;
    reading.pm10_mass = read_be16u(&frame[10]) * 0.1f;
    reading.pm0_5_count = read_be16u(&frame[12]) * 0.1f;
    reading.pm1_0_count = read_be16u(&frame[14]) * 0.1f;
    reading.pm2_5_count = read_be16u(&frame[16]) * 0.1f;
    reading.pm5_0_count = read_be16u(&frame[18]) * 0.1f;
    reading.pm10_count = read_be16u(&frame[20]) * 0.1f;
    reading.tvoc = read_be16u(&frame[22]) * 10.0f;
    reading.eco2 = read_be16u(&frame[24]) * 1.0f;
    reading.iaq = read_be16u(&frame[26]) * 0.01f;

    uint16_t status = read_be16u(&frame[28]);
    reading.fan_error = (status & 0x01) != 0;
    reading.fan_speed_error = (status & 0x02) != 0;

    return ESP_OK;
}

} // namespace rrh62000
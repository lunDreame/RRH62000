#include "matter_airq.hpp"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_core.h"
#include "esp_matter_cluster.h"
#include "esp_matter_attribute.h"
#include "esp_matter_endpoint.h"
#include <cmath>

namespace {
constexpr const char* TAG = "matter_airq";
constexpr uint16_t ENDPOINT_ID = 1;
constexpr float EPSILON_TEMP = 0.1f;
constexpr float EPSILON_RH = 1.0f;
constexpr float EPSILON_PM = 1.0f;
constexpr float EPSILON_CO2 = 50.0f;
constexpr float EPSILON_VOC = 10.0f;

chip::app::Clusters::AirQuality::AirQualityEnum convert_iaq_to_enum(float iaq, bool has_fault) {
    if (has_fault) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kUnknown;
    }
    if (iaq < 50.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kGood;
    } else if (iaq < 100.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kFair;
    } else if (iaq < 150.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kModerate;
    } else if (iaq < 200.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kPoor;
    } else if (iaq < 300.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kVeryPoor;
    } else {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kExtremelyPoor;
    }
}

struct CachedValues {
    float temperature = NAN;
    float humidity = NAN;
    float pm2_5 = NAN;
    float pm10 = NAN;
    float co2 = NAN;
    float tvoc = NAN;
    chip::app::Clusters::AirQuality::AirQualityEnum air_quality = chip::app::Clusters::AirQuality::AirQualityEnum::kUnknown;
    bool fault = false;
} cached;

esp_err_t update_temperature(int16_t endpoint_id, float value) {
    if (std::isnan(cached.temperature) || std::abs(value - cached.temperature) >= EPSILON_TEMP) {
        cached.temperature = value;
        int16_t matter_value = static_cast<int16_t>(std::round(value * 100));
        esp_matter_attr_val_t val = esp_matter_int16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::TemperatureMeasurement::Id,
                                              chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_humidity(int16_t endpoint_id, float value) {
    if (std::isnan(cached.humidity) || std::abs(value - cached.humidity) >= EPSILON_RH) {
        cached.humidity = value;
        uint16_t matter_value = static_cast<uint16_t>(std::round(value * 100));
        esp_matter_attr_val_t val = esp_matter_uint16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::RelativeHumidityMeasurement::Id,
                                              chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_pm2_5(int16_t endpoint_id, float value) {
    if (std::isnan(cached.pm2_5) || std::abs(value - cached.pm2_5) >= EPSILON_PM) {
        cached.pm2_5 = value;
        uint16_t matter_value = static_cast<uint16_t>(std::round(value * 10));
        esp_matter_attr_val_t val = esp_matter_uint16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Pm25ConcentrationMeasurement::Id,
                                              chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_pm10(int16_t endpoint_id, float value) {
    if (std::isnan(cached.pm10) || std::abs(value - cached.pm10) >= EPSILON_PM) {
        cached.pm10 = value;
        uint16_t matter_value = static_cast<uint16_t>(std::round(value * 10));
        esp_matter_attr_val_t val = esp_matter_uint16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Pm10ConcentrationMeasurement::Id,
                                              chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_co2(int16_t endpoint_id, float value) {
    if (std::isnan(cached.co2) || std::abs(value - cached.co2) >= EPSILON_CO2) {
        cached.co2 = value;
        uint16_t matter_value = static_cast<uint16_t>(std::round(value));
        esp_matter_attr_val_t val = esp_matter_uint16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id,
                                              chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_tvoc(int16_t endpoint_id, float value) {
    if (std::isnan(cached.tvoc) || std::abs(value - cached.tvoc) >= EPSILON_VOC) {
        cached.tvoc = value;
        uint16_t matter_value = static_cast<uint16_t>(std::round(value / 10));
        esp_matter_attr_val_t val = esp_matter_uint16(matter_value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Id,
                                              chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_air_quality(int16_t endpoint_id, chip::app::Clusters::AirQuality::AirQualityEnum value) {
    if (cached.air_quality != value) {
        cached.air_quality = value;
        esp_matter_attr_val_t val = esp_matter_uint8(static_cast<uint8_t>(value));
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::AirQuality::Id,
                                              chip::app::Clusters::AirQuality::Attributes::AirQuality::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_fault(int16_t endpoint_id, bool value) {
    if (cached.fault != value) {
        cached.fault = value;
        esp_matter_attr_val_t val = esp_matter_bool(value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::BooleanState::Id,
                                              chip::app::Clusters::BooleanState::Attributes::StateValue::Id,
                                              &val);
    }
    return ESP_OK;
}

} // namespace

namespace matter_airq {

esp_err_t init() {
    esp_matter::node::config_t node_config;
    esp_matter::node_t* node = esp_matter::node::create(&node_config, nullptr, nullptr);
    if (node == nullptr) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return ESP_FAIL;
    }

    esp_matter::endpoint::air_quality_sensor::config_t endpoint_config;
    esp_matter::endpoint_t* endpoint = esp_matter::endpoint::air_quality_sensor::create(node, &endpoint_config, 0, nullptr);
    if (endpoint == nullptr) {
        ESP_LOGE(TAG, "Failed to create endpoint");
        return ESP_FAIL;
    }

    esp_matter::cluster::temperature_measurement::config_t temp_config;
    esp_matter::cluster_t* temp_cluster = esp_matter::cluster::temperature_measurement::create(endpoint, &temp_config, 0);
    if (temp_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create TemperatureMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::relative_humidity_measurement::config_t rh_config;
    esp_matter::cluster_t* rh_cluster = esp_matter::cluster::relative_humidity_measurement::create(endpoint, &rh_config, 0);
    if (rh_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create RelativeHumidityMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::pm25_concentration_measurement::config_t pm25_config;
    esp_matter::cluster_t* pm25_cluster = esp_matter::cluster::pm25_concentration_measurement::create(endpoint, &pm25_config, 0);
    if (pm25_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create Pm25ConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::pm10_concentration_measurement::config_t pm10_config;
    esp_matter::cluster_t* pm10_cluster = esp_matter::cluster::pm10_concentration_measurement::create(endpoint, &pm10_config, 0);
    if (pm10_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create Pm10ConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::carbon_dioxide_concentration_measurement::config_t co2_config;
    esp_matter::cluster_t* co2_cluster = esp_matter::cluster::carbon_dioxide_concentration_measurement::create(endpoint, &co2_config, 0);
    if (co2_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create CarbonDioxideConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::total_volatile_organic_compounds_concentration_measurement::config_t tvoc_config;
    esp_matter::cluster_t* tvoc_cluster = esp_matter::cluster::total_volatile_organic_compounds_concentration_measurement::create(endpoint, &tvoc_config, 0);
    if (tvoc_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create TotalVolatileOrganicCompoundsConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::air_quality::config_t airq_config;
    esp_matter::cluster_t* airq_cluster = esp_matter::cluster::air_quality::create(endpoint, &airq_config, 0);
    if (airq_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create AirQuality cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::boolean_state::config_t bool_state_config;
    esp_matter::cluster_t* bool_state_cluster = esp_matter::cluster::boolean_state::create(endpoint, &bool_state_config, 0);
    if (bool_state_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create BooleanState cluster");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_matter::start(nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter node: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Matter Air Quality endpoint created: endpoint_id=%d", ENDPOINT_ID);
    return ESP_OK;
}

esp_err_t publish(const rrh62000::Reading& reading) {
    int16_t endpoint_id = static_cast<int16_t>(ENDPOINT_ID);
    esp_err_t ret;

    ret = update_temperature(endpoint_id, reading.temperature_c);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update temperature: %s", esp_err_to_name(ret));
    }

    ret = update_humidity(endpoint_id, reading.humidity_rh);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update humidity: %s", esp_err_to_name(ret));
    }

    ret = update_pm2_5(endpoint_id, reading.pm2_5_mass);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update PM2.5: %s", esp_err_to_name(ret));
    }

    ret = update_pm10(endpoint_id, reading.pm10_mass);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update PM10: %s", esp_err_to_name(ret));
    }

    ret = update_co2(endpoint_id, reading.eco2);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update CO2: %s", esp_err_to_name(ret));
    }

    ret = update_tvoc(endpoint_id, reading.tvoc);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update TVOC: %s", esp_err_to_name(ret));
    }

    bool fault = reading.fan_error || reading.fan_speed_error;
    chip::app::Clusters::AirQuality::AirQualityEnum air_quality = convert_iaq_to_enum(reading.iaq, fault);

    ret = update_air_quality(endpoint_id, air_quality);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update air quality: %s", esp_err_to_name(ret));
    }

    ret = update_fault(endpoint_id, fault);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update fault: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

} // namespace matter_airq
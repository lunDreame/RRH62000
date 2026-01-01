#include "matter_airq.hpp"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_core.h"
#include "esp_matter_cluster.h"
#include "esp_matter_attribute.h"
#include "esp_matter_endpoint.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include <app/server/Server.h>
#include <platform/PlatformManager.h>
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
    if (iaq <= 1.9f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kGood;
    } else if (iaq < 3.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kFair;
    } else if (iaq < 4.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kModerate;
    } else if (iaq < 5.0f) {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kPoor;
    } else {
        return chip::app::Clusters::AirQuality::AirQualityEnum::kVeryPoor;
    }
}

struct CachedValues {
    float temperature = NAN;
    float humidity = NAN;
    float pm1_0 = NAN;
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

esp_err_t update_pm1_0(int16_t endpoint_id, float value) {
    if (std::isnan(cached.pm1_0) || std::abs(value - cached.pm1_0) >= EPSILON_PM) {
        cached.pm1_0 = value;
        esp_matter_attr_val_t val = esp_matter_nullable_float(value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Pm1ConcentrationMeasurement::Id,
                                              chip::app::Clusters::Pm1ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_pm2_5(int16_t endpoint_id, float value) {
    if (std::isnan(cached.pm2_5) || std::abs(value - cached.pm2_5) >= EPSILON_PM) {
        cached.pm2_5 = value;
        esp_matter_attr_val_t val = esp_matter_nullable_float(value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Pm25ConcentrationMeasurement::Id,
                                              chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_pm10(int16_t endpoint_id, float value) {
    if (std::isnan(cached.pm10) || std::abs(value - cached.pm10) >= EPSILON_PM) {
        cached.pm10 = value;
        esp_matter_attr_val_t val = esp_matter_nullable_float(value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Pm10ConcentrationMeasurement::Id,
                                              chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_co2(int16_t endpoint_id, float value) {
    if (std::isnan(cached.co2) || std::abs(value - cached.co2) >= EPSILON_CO2) {
        cached.co2 = value;
        esp_matter_attr_val_t val = esp_matter_nullable_float(value);
        return esp_matter::attribute::update(endpoint_id, chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id,
                                              chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
                                              &val);
    }
    return ESP_OK;
}

esp_err_t update_tvoc(int16_t endpoint_id, float value) {
    if (std::isnan(cached.tvoc) || std::abs(value - cached.tvoc) >= EPSILON_VOC) {
        cached.tvoc = value;
        esp_matter_attr_val_t val = esp_matter_nullable_float(value);
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
    esp_matter::endpoint_t *endpoint = esp_matter::endpoint::air_quality_sensor::create(node, &endpoint_config, esp_matter::ENDPOINT_FLAG_DESTROYABLE, nullptr);
    if (endpoint == nullptr) {
        ESP_LOGE(TAG, "Failed to create endpoint");
        return ESP_FAIL;
    }

    esp_matter::cluster::temperature_measurement::config_t temp_config;
    esp_matter::cluster_t *temp_cluster = esp_matter::cluster::temperature_measurement::create(endpoint, &temp_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (temp_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create TemperatureMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::relative_humidity_measurement::config_t rh_config;
    esp_matter::cluster_t *rh_cluster = esp_matter::cluster::relative_humidity_measurement::create(endpoint, &rh_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (rh_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create RelativeHumidityMeasurement cluster");
        return ESP_FAIL;
    }

    // Note: PM0.3, PM0.5, PM5.0 are not standard Matter clusters, so we only add PM1.0
    esp_matter::cluster::pm1_concentration_measurement::config_t pm1_config;
    esp_matter::cluster_t *pm1_cluster = esp_matter::cluster::pm1_concentration_measurement::create(endpoint, &pm1_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (pm1_cluster == nullptr) {
        ESP_LOGW(TAG, "PM1.0 cluster not supported, skipping...");
    } else {
        // Set FeatureMap for PM1.0 cluster
        esp_matter::attribute_t *pm1_feature_map_attr = esp_matter::attribute::get(pm1_cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
        if (pm1_feature_map_attr) {
            esp_matter_attr_val_t pm1_feature_map_val;
            esp_err_t ret = esp_matter::attribute::get_val(pm1_feature_map_attr, &pm1_feature_map_val);
            if (ret == ESP_OK) {
                pm1_feature_map_val.val.u32 |= 0x1; // MEA: Cluster supports numeric measurement of substance
                ret = esp_matter::attribute::set_val(pm1_feature_map_attr, &pm1_feature_map_val);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to set FeatureMap for PM1.0 cluster: %s", esp_err_to_name(ret));
                } else {
                    ESP_LOGI(TAG, "FeatureMap set for PM1.0 cluster (MEA bit enabled)");
                }
            }
        }

        // Create PM1.0 cluster attributes manually
        esp_matter::attribute_t *pm1_attr;
        uint32_t pm1_attr_id;
        uint8_t pm1_flags;

        // Create MeasuredValue attribute
        pm1_attr_id = chip::app::Clusters::Pm1ConcentrationMeasurement::Attributes::MeasuredValue::Id;
        pm1_attr = esp_matter::attribute::get(pm1_cluster, pm1_attr_id);
        if (!pm1_attr) {
            pm1_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
            pm1_attr = esp_matter::attribute::create(pm1_cluster, pm1_attr_id, pm1_flags, esp_matter_nullable_float(nullable<float>()));
            if (!pm1_attr) {
                ESP_LOGE(TAG, "Failed to create PM1.0 MeasuredValue attribute");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "PM1.0 MeasuredValue attribute created");
        }

        // Create MinMeasuredValue attribute
        pm1_attr_id = chip::app::Clusters::Pm1ConcentrationMeasurement::Attributes::MinMeasuredValue::Id;
        pm1_attr = esp_matter::attribute::get(pm1_cluster, pm1_attr_id);
        if (!pm1_attr) {
            pm1_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
            pm1_attr = esp_matter::attribute::create(pm1_cluster, pm1_attr_id, pm1_flags, esp_matter_nullable_float(nullable<float>()));
            if (!pm1_attr) {
                ESP_LOGE(TAG, "Failed to create PM1.0 MinMeasuredValue attribute");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "PM1.0 MinMeasuredValue attribute created");
        }

        // Create MaxMeasuredValue attribute
        pm1_attr_id = chip::app::Clusters::Pm1ConcentrationMeasurement::Attributes::MaxMeasuredValue::Id;
        pm1_attr = esp_matter::attribute::get(pm1_cluster, pm1_attr_id);
        if (!pm1_attr) {
            pm1_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
            pm1_attr = esp_matter::attribute::create(pm1_cluster, pm1_attr_id, pm1_flags, esp_matter_nullable_float(nullable<float>()));
            if (!pm1_attr) {
                ESP_LOGE(TAG, "Failed to create PM1.0 MaxMeasuredValue attribute");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "PM1.0 MaxMeasuredValue attribute created");
        }

        // Create MeasurementUnit attribute
        pm1_attr_id = chip::app::Clusters::Pm1ConcentrationMeasurement::Attributes::MeasurementUnit::Id;
        pm1_attr = esp_matter::attribute::get(pm1_cluster, pm1_attr_id);
        if (!pm1_attr) {
            pm1_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NONE;
            pm1_attr = esp_matter::attribute::create(pm1_cluster, pm1_attr_id, pm1_flags, esp_matter_enum8(4)); // µg/m³ (kUgm3)
            if (!pm1_attr) {
                ESP_LOGE(TAG, "Failed to create PM1.0 MeasurementUnit attribute");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "PM1.0 MeasurementUnit attribute created (µg/m³)");
        }
    }

    esp_matter::cluster::pm25_concentration_measurement::config_t pm25_config;
    esp_matter::cluster_t *pm25_cluster = esp_matter::cluster::pm25_concentration_measurement::create(endpoint, &pm25_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (pm25_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create Pm25ConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    // Set FeatureMap for PM2.5 cluster
    esp_matter::attribute_t *pm25_feature_map_attr = esp_matter::attribute::get(pm25_cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
    if (pm25_feature_map_attr) {
        esp_matter_attr_val_t pm25_feature_map_val;
        esp_err_t ret = esp_matter::attribute::get_val(pm25_feature_map_attr, &pm25_feature_map_val);
        if (ret == ESP_OK) {
            pm25_feature_map_val.val.u32 |= 0x1; // MEA: Cluster supports numeric measurement of substance
            ret = esp_matter::attribute::set_val(pm25_feature_map_attr, &pm25_feature_map_val);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set FeatureMap for PM2.5 cluster: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "FeatureMap set for PM2.5 cluster (MEA bit enabled)");
            }
        }
    }

    // Create PM2.5 cluster attributes manually
    esp_matter::attribute_t *pm25_attr;
    uint32_t pm25_attr_id;
    uint8_t pm25_flags;

    // Create MeasuredValue attribute
    pm25_attr_id = chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id;
    pm25_attr = esp_matter::attribute::get(pm25_cluster, pm25_attr_id);
    if (!pm25_attr) {
        pm25_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm25_attr = esp_matter::attribute::create(pm25_cluster, pm25_attr_id, pm25_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm25_attr) {
            ESP_LOGE(TAG, "Failed to create PM2.5 MeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM2.5 MeasuredValue attribute created");
    }

    // Create MinMeasuredValue attribute
    pm25_attr_id = chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MinMeasuredValue::Id;
    pm25_attr = esp_matter::attribute::get(pm25_cluster, pm25_attr_id);
    if (!pm25_attr) {
        pm25_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm25_attr = esp_matter::attribute::create(pm25_cluster, pm25_attr_id, pm25_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm25_attr) {
            ESP_LOGE(TAG, "Failed to create PM2.5 MinMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM2.5 MinMeasuredValue attribute created");
    }

    // Create MaxMeasuredValue attribute
    pm25_attr_id = chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MaxMeasuredValue::Id;
    pm25_attr = esp_matter::attribute::get(pm25_cluster, pm25_attr_id);
    if (!pm25_attr) {
        pm25_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm25_attr = esp_matter::attribute::create(pm25_cluster, pm25_attr_id, pm25_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm25_attr) {
            ESP_LOGE(TAG, "Failed to create PM2.5 MaxMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM2.5 MaxMeasuredValue attribute created");
    }

    // Create MeasurementUnit attribute
    pm25_attr_id = chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MeasurementUnit::Id;
    pm25_attr = esp_matter::attribute::get(pm25_cluster, pm25_attr_id);
    if (!pm25_attr) {
        pm25_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NONE;
        pm25_attr = esp_matter::attribute::create(pm25_cluster, pm25_attr_id, pm25_flags, esp_matter_enum8(4)); // µg/m³ (kUgm3)
        if (!pm25_attr) {
            ESP_LOGE(TAG, "Failed to create PM2.5 MeasurementUnit attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM2.5 MeasurementUnit attribute created (µg/m³)");
    }

    esp_matter::cluster::pm10_concentration_measurement::config_t pm10_config;
    esp_matter::cluster_t *pm10_cluster = esp_matter::cluster::pm10_concentration_measurement::create(endpoint, &pm10_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (pm10_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create Pm10ConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    // Set FeatureMap for PM10 cluster
    esp_matter::attribute_t *pm10_feature_map_attr = esp_matter::attribute::get(pm10_cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
    if (pm10_feature_map_attr) {
        esp_matter_attr_val_t pm10_feature_map_val;
        esp_err_t ret = esp_matter::attribute::get_val(pm10_feature_map_attr, &pm10_feature_map_val);
        if (ret == ESP_OK) {
            pm10_feature_map_val.val.u32 |= 0x1; // MEA: Cluster supports numeric measurement of substance
            ret = esp_matter::attribute::set_val(pm10_feature_map_attr, &pm10_feature_map_val);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set FeatureMap for PM10 cluster: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "FeatureMap set for PM10 cluster (MEA bit enabled)");
            }
        }
    }

    // Create PM10 cluster attributes manually
    esp_matter::attribute_t *pm10_attr;
    uint32_t pm10_attr_id;
    uint8_t pm10_flags;

    // Create MeasuredValue attribute
    pm10_attr_id = chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MeasuredValue::Id;
    pm10_attr = esp_matter::attribute::get(pm10_cluster, pm10_attr_id);
    if (!pm10_attr) {
        pm10_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm10_attr = esp_matter::attribute::create(pm10_cluster, pm10_attr_id, pm10_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm10_attr) {
            ESP_LOGE(TAG, "Failed to create PM10 MeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM10 MeasuredValue attribute created");
    }

    // Create MinMeasuredValue attribute
    pm10_attr_id = chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MinMeasuredValue::Id;
    pm10_attr = esp_matter::attribute::get(pm10_cluster, pm10_attr_id);
    if (!pm10_attr) {
        pm10_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm10_attr = esp_matter::attribute::create(pm10_cluster, pm10_attr_id, pm10_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm10_attr) {
            ESP_LOGE(TAG, "Failed to create PM10 MinMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM10 MinMeasuredValue attribute created");
    }

    // Create MaxMeasuredValue attribute
    pm10_attr_id = chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MaxMeasuredValue::Id;
    pm10_attr = esp_matter::attribute::get(pm10_cluster, pm10_attr_id);
    if (!pm10_attr) {
        pm10_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        pm10_attr = esp_matter::attribute::create(pm10_cluster, pm10_attr_id, pm10_flags, esp_matter_nullable_float(nullable<float>()));
        if (!pm10_attr) {
            ESP_LOGE(TAG, "Failed to create PM10 MaxMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM10 MaxMeasuredValue attribute created");
    }

    // Create MeasurementUnit attribute
    pm10_attr_id = chip::app::Clusters::Pm10ConcentrationMeasurement::Attributes::MeasurementUnit::Id;
    pm10_attr = esp_matter::attribute::get(pm10_cluster, pm10_attr_id);
    if (!pm10_attr) {
        pm10_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NONE;
        pm10_attr = esp_matter::attribute::create(pm10_cluster, pm10_attr_id, pm10_flags, esp_matter_enum8(4)); // µg/m³ (kUgm3)
        if (!pm10_attr) {
            ESP_LOGE(TAG, "Failed to create PM10 MeasurementUnit attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "PM10 MeasurementUnit attribute created (µg/m³)");
    }

    esp_matter::cluster::carbon_dioxide_concentration_measurement::config_t co2_config;
    esp_matter::cluster_t *co2_cluster = esp_matter::cluster::carbon_dioxide_concentration_measurement::create(endpoint, &co2_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (co2_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create CarbonDioxideConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::attribute_t *feature_map_attr = esp_matter::attribute::get(co2_cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
    if (feature_map_attr) {
        esp_matter_attr_val_t feature_map_val;
        esp_err_t ret = esp_matter::attribute::get_val(feature_map_attr, &feature_map_val);
        if (ret == ESP_OK) {
            feature_map_val.val.u32 |= 0x1; // MEA: Cluster supports numeric measurement of substance
            ret = esp_matter::attribute::set_val(feature_map_attr, &feature_map_val);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set FeatureMap for CO2 cluster: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "FeatureMap set for CO2 cluster (MEA bit enabled)");
            }
        }
    }

    // Create CO2 cluster attributes manually
    esp_matter::attribute_t *attr;
    uint32_t attr_id;
    uint8_t flags;

    // Create MeasuredValue attribute
    attr_id = chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id;
    attr = esp_matter::attribute::get(co2_cluster, attr_id);
    if (!attr) {
        flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        attr = esp_matter::attribute::create(co2_cluster, attr_id, flags, esp_matter_nullable_float(nullable<float>()));
        if (!attr) {
            ESP_LOGE(TAG, "Failed to create CO2 MeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "CO2 MeasuredValue attribute created");
    }

    // Create MinMeasuredValue attribute
    attr_id = chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MinMeasuredValue::Id;
    attr = esp_matter::attribute::get(co2_cluster, attr_id);
    if (!attr) {
        flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        attr = esp_matter::attribute::create(co2_cluster, attr_id, flags, esp_matter_nullable_float(nullable<float>()));
        if (!attr) {
            ESP_LOGE(TAG, "Failed to create CO2 MinMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "CO2 MinMeasuredValue attribute created");
    }

    // Create MaxMeasuredValue attribute
    attr_id = chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MaxMeasuredValue::Id;
    attr = esp_matter::attribute::get(co2_cluster, attr_id);
    if (!attr) {
        flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        attr = esp_matter::attribute::create(co2_cluster, attr_id, flags, esp_matter_nullable_float(nullable<float>()));
        if (!attr) {
            ESP_LOGE(TAG, "Failed to create CO2 MaxMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "CO2 MaxMeasuredValue attribute created");
    }

    // Create MeasurementUnit attribute
    attr_id = chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MeasurementUnit::Id;
    attr = esp_matter::attribute::get(co2_cluster, attr_id);
    if (!attr) {
        flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NONE;
        attr = esp_matter::attribute::create(co2_cluster, attr_id, flags, esp_matter_enum8(0)); // PPM
        if (!attr) {
            ESP_LOGE(TAG, "Failed to create CO2 MeasurementUnit attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "CO2 MeasurementUnit attribute created");
    }

    esp_matter::cluster::total_volatile_organic_compounds_concentration_measurement::config_t tvoc_config;
    esp_matter::cluster_t *tvoc_cluster = esp_matter::cluster::total_volatile_organic_compounds_concentration_measurement::create(endpoint, &tvoc_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (tvoc_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create TotalVolatileOrganicCompoundsConcentrationMeasurement cluster");
        return ESP_FAIL;
    }

    esp_matter::attribute_t *tvoc_feature_map_attr = esp_matter::attribute::get(tvoc_cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
    if (tvoc_feature_map_attr) {
        esp_matter_attr_val_t tvoc_feature_map_val;
        esp_err_t ret = esp_matter::attribute::get_val(tvoc_feature_map_attr, &tvoc_feature_map_val);
        if (ret == ESP_OK) {
            tvoc_feature_map_val.val.u32 |= 0x1; // MEA: Cluster supports numeric measurement of substance
            ret = esp_matter::attribute::set_val(tvoc_feature_map_attr, &tvoc_feature_map_val);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set FeatureMap for TVOC cluster: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "FeatureMap set for TVOC cluster (MEA bit enabled)");
            }
        }
    }

    // Create TVOC cluster attributes manually
    esp_matter::attribute_t *tvoc_attr;
    uint32_t tvoc_attr_id;
    uint8_t tvoc_flags;

    // Create MeasuredValue attribute
    tvoc_attr_id = chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasuredValue::Id;
    tvoc_attr = esp_matter::attribute::get(tvoc_cluster, tvoc_attr_id);
    if (!tvoc_attr) {
        tvoc_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        tvoc_attr = esp_matter::attribute::create(tvoc_cluster, tvoc_attr_id, tvoc_flags, esp_matter_nullable_float(nullable<float>()));
        if (!tvoc_attr) {
            ESP_LOGE(TAG, "Failed to create TVOC MeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "TVOC MeasuredValue attribute created");
    }

    // Create MinMeasuredValue attribute
    tvoc_attr_id = chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MinMeasuredValue::Id;
    tvoc_attr = esp_matter::attribute::get(tvoc_cluster, tvoc_attr_id);
    if (!tvoc_attr) {
        tvoc_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        tvoc_attr = esp_matter::attribute::create(tvoc_cluster, tvoc_attr_id, tvoc_flags, esp_matter_nullable_float(nullable<float>()));
        if (!tvoc_attr) {
            ESP_LOGE(TAG, "Failed to create TVOC MinMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "TVOC MinMeasuredValue attribute created");
    }

    // Create MaxMeasuredValue attribute
    tvoc_attr_id = chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MaxMeasuredValue::Id;
    tvoc_attr = esp_matter::attribute::get(tvoc_cluster, tvoc_attr_id);
    if (!tvoc_attr) {
        tvoc_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NULLABLE;
        tvoc_attr = esp_matter::attribute::create(tvoc_cluster, tvoc_attr_id, tvoc_flags, esp_matter_nullable_float(nullable<float>()));
        if (!tvoc_attr) {
            ESP_LOGE(TAG, "Failed to create TVOC MaxMeasuredValue attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "TVOC MaxMeasuredValue attribute created");
    }

    // Create MeasurementUnit attribute
    tvoc_attr_id = chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasurementUnit::Id;
    tvoc_attr = esp_matter::attribute::get(tvoc_cluster, tvoc_attr_id);
    if (!tvoc_attr) {
        tvoc_flags = esp_matter::attribute_flags::ATTRIBUTE_FLAG_NONE;
        tvoc_attr = esp_matter::attribute::create(tvoc_cluster, tvoc_attr_id, tvoc_flags, esp_matter_enum8(4)); // µg/m³ (kUgm3)
        if (!tvoc_attr) {
            ESP_LOGE(TAG, "Failed to create TVOC MeasurementUnit attribute");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "TVOC MeasurementUnit attribute created (µg/m³)");
    }

    esp_matter::cluster::air_quality::config_t airq_config;
    esp_matter::cluster_t *airq_cluster = esp_matter::cluster::air_quality::create(endpoint, &airq_config, esp_matter::CLUSTER_FLAG_SERVER);
    if (airq_cluster == nullptr) {
        ESP_LOGE(TAG, "Failed to create AirQuality cluster");
        return ESP_FAIL;
    }

    esp_matter::cluster::boolean_state::config_t bool_state_config;
    esp_matter::cluster_t *bool_state_cluster = esp_matter::cluster::boolean_state::create(endpoint, &bool_state_config, esp_matter::CLUSTER_FLAG_SERVER);
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

    ret = update_pm1_0(endpoint_id, reading.pm1_0_mass);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update PM1.0: %s", esp_err_to_name(ret));
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

namespace {
void factory_reset_work_handler(intptr_t context) {
    ESP_LOGI(TAG, "Performing factory reset...");

    chip::Server::GetInstance().GetFabricTable().DeleteAllFabrics();

    const char* matter_namespaces[] = {
        "chip-factory",
        "chip-config",
        "chip-counters",
        "chip-kvs",
        "chip-account",
        "chip-binding"
    };

    for (const char* ns : matter_namespaces) {
        nvs_handle_t handle;
        esp_err_t ret = nvs_open(ns, NVS_READWRITE, &handle);
        if (ret == ESP_OK) {
            nvs_erase_all(handle);
            nvs_commit(handle);
            nvs_close(handle);
            ESP_LOGI(TAG, "Erased NVS namespace: %s", ns);
        } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to open NVS namespace %s: %s", ns, esp_err_to_name(ret));
        }
    }

    nvs_handle_t wifi_handle;
    esp_err_t ret = nvs_open("nvs.net80211", NVS_READWRITE, &wifi_handle);
    if (ret == ESP_OK) {
        nvs_erase_all(wifi_handle);
        nvs_commit(wifi_handle);
        nvs_close(wifi_handle);
        ESP_LOGI(TAG, "Erased NVS namespace: nvs.net80211");
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to open NVS namespace nvs.net80211: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Factory reset completed. Restarting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}
} // anonymous namespace

esp_err_t factory_reset() {
    ESP_LOGI(TAG, "Scheduling factory reset on CHIP main thread...");
    chip::DeviceLayer::PlatformMgr().ScheduleWork(factory_reset_work_handler, 0);
    return ESP_OK;
}

} // namespace matter_airq
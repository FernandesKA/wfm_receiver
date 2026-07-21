/**
 * @file rds_decoder_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the RDS (Radio Data System) bit recovery stage
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace config {

    struct rds_decoder_config {
        uint32_t sample_rate_hz = 200'000; // rate of the composite input (post-demod, pre-audio-resample)
        double pilot_hz = 19'000.0;        // stereo pilot; RDS subcarrier is locked to 3x this
        double subcarrier_hz = 57'000.0;   // RDS subcarrier (3 * pilot_hz)
        double symbol_rate_hz = 1187.5;    // RDS biphase symbol rate
        std::size_t oversample = 8;        // samples per symbol after internal resampling
    };

} // namespace config

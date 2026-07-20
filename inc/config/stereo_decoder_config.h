/**
 * @file stereo_decoder_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the WFM stereo (pilot/DSB-SC) decoder
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace config {

    struct stereo_decoder_config {
        uint32_t sample_rate_hz = 200'000; // rate of the composite input (post-demod, pre-audio-resample)
        double pilot_hz = 19'000.0;        // broadcast WFM stereo pilot tone
        double audio_bandwidth_hz = 15'000.0; // L+R / L-R lowpass cutoff
    };

} // namespace config

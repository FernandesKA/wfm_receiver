/**
 * @file resampler_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for a rational sample rate converter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace config {

    // Shared by both resampling stages in the receive chain: IQ decimation
    // (wideband capture rate down toward the channel bandwidth) and the
    // final audio resampling stage (demodulated signal down to the sound
    // card's rate) - both just need an input/output rate pair.
    struct resampler_config {
        uint32_t input_sample_rate_hz = 2'000'000;
        uint32_t output_sample_rate_hz = 200'000;
    };

} // namespace config

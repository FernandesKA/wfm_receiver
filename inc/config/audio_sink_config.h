/**
 * @file audio_sink_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the audio playback sink
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <string>

namespace config {

    struct audio_sink_config {
        std::string device = "default"; // ALSA PCM device name
        uint32_t sample_rate_hz = 48'000;
        uint32_t channels = 1; // mono
    };

} // namespace config

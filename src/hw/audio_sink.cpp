/**
 * @file audio_sink.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Factory for creating an audio_sink from config
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hw/audio_sink.h"

#include "hw/alsa_sink.h"

namespace hardware {

    std::unique_ptr<audio_sink> create_audio_sink(const config::audio_sink_config &cfg) {
        return std::make_unique<alsa_sink>(cfg);
    }

} // namespace hardware

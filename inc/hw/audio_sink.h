/**
 * @file audio_sink.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Abstract interface for a PCM audio playback sink
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <memory>

#include "config/audio_sink_config.h"

namespace hardware {

    // Consumes float samples in [-1, 1] and plays them through the system's
    // audio output. Mirrors signal_source on the RX side: an open/close-
    // managed device abstraction, one concrete implementation per backend
    // (currently only ALSA).
    class audio_sink {
    public:
        explicit audio_sink(const config::audio_sink_config &cfg) : m_config(cfg) {}
        virtual ~audio_sink() = default;

        audio_sink(const audio_sink &) = delete;
        audio_sink &operator=(const audio_sink &) = delete;

        virtual bool open() = 0;

        // Blocking write of `count` float samples in [-1, 1]. For
        // cfg.channels > 1, samples must be interleaved (L,R,L,R,... for
        // stereo) and `count` is the total sample count (channels * frames),
        // not the frame count.
        virtual bool write(const float *samples, std::size_t count) = 0;

        virtual void close() = 0;

    protected:
        config::audio_sink_config m_config;
    };

    std::unique_ptr<audio_sink> create_audio_sink(const config::audio_sink_config &cfg);

} // namespace hardware

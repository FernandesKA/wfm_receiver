/**
 * @file alsa_sink.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief ALSA implementation of the audio playback sink
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <alsa/asoundlib.h>

#include "hw/audio_sink.h"

namespace hardware {

    class alsa_sink final : public audio_sink {
    public:
        explicit alsa_sink(const config::audio_sink_config &cfg);
        ~alsa_sink() override;

        bool open() override;
        bool write(const float *samples, std::size_t count) override;
        void close() override;

    private:
        snd_pcm_t *m_pcm = nullptr;
    };

} // namespace hardware

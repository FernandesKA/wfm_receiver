/**
 * @file alsa_sink.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief ALSA implementation of the audio playback sink
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hw/alsa_sink.h"

#include <cerrno>
#include <cstdio>

namespace hardware {

    namespace {
        bool check(int result, const char *what) {
            if (result < 0) {
                std::fprintf(stderr, "alsa_sink: %s failed: %s\n", what, snd_strerror(result));
                return false;
            }
            return true;
        }
    } // namespace

    alsa_sink::alsa_sink(const config::audio_sink_config &cfg) : audio_sink(cfg) {}

    alsa_sink::~alsa_sink() {
        close();
    }

    bool alsa_sink::open() {
        if (m_pcm != nullptr) {
            return true;
        }

        if (!check(snd_pcm_open(&m_pcm, m_config.device.c_str(), SND_PCM_STREAM_PLAYBACK, 0), "snd_pcm_open")) {
            m_pcm = nullptr;
            return false;
        }

        // snd_pcm_set_params negotiates hw_params for us with sane defaults -
        // plenty for straightforward mono playback, no need for manual
        // hw_params/sw_params tuning here.
        const int result = snd_pcm_set_params(m_pcm, SND_PCM_FORMAT_FLOAT, SND_PCM_ACCESS_RW_INTERLEAVED,
                                               m_config.channels, m_config.sample_rate_hz,
                                               1,      // allow ALSA to soft-resample if the device can't do our rate
                                               200000  // 200 ms latency
        );
        if (!check(result, "snd_pcm_set_params")) {
            snd_pcm_close(m_pcm);
            m_pcm = nullptr;
            return false;
        }

        return true;
    }

    bool alsa_sink::write(const float *samples, std::size_t count) {
        if (m_pcm == nullptr) {
            return false;
        }

        std::size_t written = 0;
        while (written < count) {
            const snd_pcm_sframes_t result =
                snd_pcm_writei(m_pcm, samples + written, static_cast<snd_pcm_uframes_t>(count - written));

            if (result == -EPIPE) {
                std::fprintf(stderr, "alsa_sink: underrun (XRUN), recovering\n");
                snd_pcm_prepare(m_pcm); // underrun: recover and retry
                continue;
            }
            if (result < 0) {
                std::fprintf(stderr, "alsa_sink: snd_pcm_writei failed: %s\n", snd_strerror(static_cast<int>(result)));
                return false;
            }

            written += static_cast<std::size_t>(result);
        }

        return true;
    }

    void alsa_sink::close() {
        if (m_pcm != nullptr) {
            snd_pcm_drain(m_pcm);
            snd_pcm_close(m_pcm);
            m_pcm = nullptr;
        }
    }

} // namespace hardware

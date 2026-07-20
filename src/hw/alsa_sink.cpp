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

        // Latency is 1 s: some sources (e.g. PlutoSDR over its network/RNDIS
        // transport) have periodic ~200-250 ms stalls in data delivery;
        // without a buffer bigger than that, they starve ALSA (XRUNs).
        const int result = snd_pcm_set_params(m_pcm, SND_PCM_FORMAT_FLOAT, SND_PCM_ACCESS_RW_INTERLEAVED,
                                               m_config.channels, m_config.sample_rate_hz,
                                               1,       // allow ALSA to soft-resample if the device can't do our rate
                                               1000000  // 1 s latency
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

        // snd_pcm_writei counts frames (one sample per channel), not raw
        // float values - for mono the two happen to coincide, but for
        // stereo `count` (interleaved L,R,L,R,...) is 2x the frame count.
        const std::size_t channels = m_config.channels;
        const std::size_t frames_total = count / channels;
        std::size_t frames_written = 0;

        while (frames_written < frames_total) {
            const snd_pcm_sframes_t result = snd_pcm_writei(m_pcm, samples + frames_written * channels,
                                                             static_cast<snd_pcm_uframes_t>(frames_total - frames_written));

            if (result == -EPIPE) {
                std::fprintf(stderr, "alsa_sink: underrun (XRUN), recovering\n");
                snd_pcm_prepare(m_pcm); // underrun: recover and retry
                continue;
            }
            if (result < 0) {
                std::fprintf(stderr, "alsa_sink: snd_pcm_writei failed: %s\n", snd_strerror(static_cast<int>(result)));
                return false;
            }

            frames_written += static_cast<std::size_t>(result);
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

/**
 * @file audio_resampler.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Rational sample rate converter for the demodulated audio signal
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <vector>

#include "config/resampler_config.h"
#include "dsp/resampler.h"

namespace dsp {

    // Rational resampler: interpolate by L, decimate by M (L/M in lowest
    // terms via gcd of cfg's rates), using the standard polyphase
    // interpolation+decimation formula - zero-stuffed samples are never
    // materialized, only taps landing on real samples are used.
    //
    // Prototype lowpass is a Blackman-windowed sinc, cutoff at
    // 0.45 * min(input_rate, output_rate): doubles as the anti-aliasing
    // filter and, for the audio leg, an audio-bandwidth limiter that also
    // removes the WFM pilot/subcarrier.
    class audio_resampler final : public resampler<float> {
    public:
        explicit audio_resampler(const config::resampler_config &cfg);

        void process(const float *in, std::size_t count, std::vector<float> &out) override;

    private:
        std::size_t m_interp_l = 1;
        std::size_t m_decim_m = 1;
        std::size_t m_taps_per_phase = 0;
        std::vector<float> m_taps; // polyphase-ordered: m_taps[phase * m_taps_per_phase + k]

        std::vector<float> m_history; // ring buffer, size == m_taps_per_phase
        std::size_t m_history_pos = 0;

        std::size_t m_phase = 0;
        std::size_t m_base = 0;
        std::size_t m_samples_pushed = 0;
    };

} // namespace dsp

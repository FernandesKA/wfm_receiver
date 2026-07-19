/**
 * @file audio_resampler.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Rational sample rate converter for the demodulated audio signal
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/audio_resampler.h"

#include <algorithm>
#include <cmath>

namespace dsp {

    namespace {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr std::size_t kTapsPerPhase = 128;

        std::size_t gcd(std::size_t a, std::size_t b) {
            while (b != 0) {
                const std::size_t t = b;
                b = a % b;
                a = t;
            }
            return a;
        }

        float sinc(float x) {
            if (std::abs(x) < 1e-8f) {
                return 1.0f;
            }
            const float px = kPi * x;
            return std::sin(px) / px;
        }

    } // namespace

    audio_resampler::audio_resampler(const config::resampler_config &cfg) {
        const std::size_t g = gcd(cfg.input_sample_rate_hz, cfg.output_sample_rate_hz);
        m_interp_l = cfg.output_sample_rate_hz / g;
        m_decim_m = cfg.input_sample_rate_hz / g;

        m_taps_per_phase = kTapsPerPhase;
        const std::size_t n_taps = m_taps_per_phase * m_interp_l;

        const float fs_up = static_cast<float>(cfg.input_sample_rate_hz) * static_cast<float>(m_interp_l);
        const float cutoff_hz =
            0.45f * static_cast<float>(std::min(cfg.input_sample_rate_hz, cfg.output_sample_rate_hz));
        const float fc_norm = cutoff_hz / fs_up;

        std::vector<float> prototype(n_taps);
        double gain_sum = 0.0;
        for (std::size_t n = 0; n < n_taps; ++n) {
            const float center = (static_cast<float>(n_taps) - 1.0f) / 2.0f;
            const float x = static_cast<float>(n) - center;
            const float ideal = 2.0f * fc_norm * sinc(2.0f * fc_norm * x);
            const float window = 0.42f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(n) / (n_taps - 1)) +
                                  0.08f * std::cos(4.0f * kPi * static_cast<float>(n) / (n_taps - 1));
            prototype[n] = ideal * window;
            gain_sum += prototype[n];
        }

        // Normalize so the prototype's DC gain is L: compensates for the
        // amplitude loss zero-stuffing introduces during interpolation, so
        // the resampler is unity-gain end-to-end.
        const float norm = static_cast<float>(m_interp_l) / static_cast<float>(gain_sum);
        for (auto &t : prototype) {
            t *= norm;
        }

        // Polyphase decomposition: branch r uses h[r], h[r+L], h[r+2L], ...
        m_taps.resize(n_taps);
        for (std::size_t r = 0; r < m_interp_l; ++r) {
            for (std::size_t k = 0; k < m_taps_per_phase; ++k) {
                m_taps[r * m_taps_per_phase + k] = prototype[r + k * m_interp_l];
            }
        }

        m_history.assign(m_taps_per_phase, 0.0f);
    }

    void audio_resampler::process(const float *in, std::size_t count, std::vector<float> &out) {
        out.clear();

        for (std::size_t n = 0; n < count; ++n) {
            m_history[m_history_pos] = in[n];
            m_history_pos = (m_history_pos + 1) % m_taps_per_phase;
            ++m_samples_pushed;

            while (m_base < m_samples_pushed) {
                float acc = 0.0f;
                const float *branch = &m_taps[m_phase * m_taps_per_phase];
                for (std::size_t k = 0; k < m_taps_per_phase; ++k) {
                    const std::size_t pos = (m_history_pos + m_taps_per_phase - 1 - k) % m_taps_per_phase;
                    acc += branch[k] * m_history[pos];
                }
                out.push_back(acc);

                const std::size_t sum = m_phase + m_decim_m;
                const std::size_t carry = sum / m_interp_l;
                m_phase = sum % m_interp_l;
                m_base += carry;
            }
        }
    }

} // namespace dsp

/**
 * @file stereo_decoder.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief WFM stereo (pilot-locked DSB-SC) decoder
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/stereo_decoder.h"

#include <cmath>
#include <utility>

namespace dsp {

    namespace {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr std::size_t kPilotBpfTaps = 401;
        constexpr std::size_t kAudioLpfTaps = 129;

        float sinc(float x) {
            if (std::abs(x) < 1e-8f) {
                return 1.0f;
            }
            const float px = kPi * x;
            return std::sin(px) / px;
        }

        // Windowed-sinc (Blackman) lowpass, unity DC gain, cutoff fc_hz at fs_hz.
        std::vector<float> design_lowpass(double fs_hz, double fc_hz, std::size_t n_taps) {
            std::vector<float> taps(n_taps);
            const float fc_norm = static_cast<float>(fc_hz / fs_hz);

            double gain_sum = 0.0;
            for (std::size_t n = 0; n < n_taps; ++n) {
                const float center = (static_cast<float>(n_taps) - 1.0f) / 2.0f;
                const float x = static_cast<float>(n) - center;
                const float ideal = 2.0f * fc_norm * sinc(2.0f * fc_norm * x);
                const float window = 0.42f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(n) / (n_taps - 1)) +
                                      0.08f * std::cos(4.0f * kPi * static_cast<float>(n) / (n_taps - 1));
                taps[n] = ideal * window;
                gain_sum += taps[n];
            }

            const float norm = 1.0f / static_cast<float>(gain_sum);
            for (auto &t : taps) {
                t *= norm;
            }
            return taps;
        }

        // Bandpass [f_lo, f_hi] via spectral subtraction of two lowpass designs.
        std::vector<float> design_bandpass(double fs_hz, double f_lo, double f_hi, std::size_t n_taps) {
            const auto lp_hi = design_lowpass(fs_hz, f_hi, n_taps);
            const auto lp_lo = design_lowpass(fs_hz, f_lo, n_taps);

            std::vector<float> taps(n_taps);
            for (std::size_t i = 0; i < n_taps; ++i) {
                taps[i] = lp_hi[i] - lp_lo[i];
            }
            return taps;
        }

    } // namespace

    stereo_decoder::fir_stage::fir_stage(std::vector<float> taps)
        : m_taps(std::move(taps)), m_delay(m_taps.size(), 0.0f) {}

    float stereo_decoder::fir_stage::push(float sample) {
        m_delay[m_pos] = sample;
        m_pos = (m_pos + 1) % m_delay.size();

        float acc = 0.0f;
        const std::size_t n = m_taps.size();
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t idx = (m_pos + n - 1 - k) % n;
            acc += m_taps[k] * m_delay[idx];
        }
        return acc;
    }

    stereo_decoder::stereo_decoder(const config::stereo_decoder_config &cfg)
        : m_pilot_bpf(design_bandpass(cfg.sample_rate_hz, cfg.pilot_hz - 1000.0, cfg.pilot_hz + 1000.0,
                                       kPilotBpfTaps)),
          m_sum_lpf(design_lowpass(cfg.sample_rate_hz, cfg.audio_bandwidth_hz, kAudioLpfTaps)),
          m_diff_lpf(design_lowpass(cfg.sample_rate_hz, cfg.audio_bandwidth_hz, kAudioLpfTaps)) {
        m_pll_freq = 2.0 * M_PI * cfg.pilot_hz / cfg.sample_rate_hz;
        // PI loop filter gains: narrow enough to reject noise/reject the
        // L+R and L-R energy leaking past the pilot bandpass, wide enough to
        // lock within a fraction of a second. Tuned empirically against a
        // synthetic composite signal.
        m_pll_kp = 0.01;
        m_pll_ki = 0.0001;
    }

    void stereo_decoder::process(const float *in, std::size_t count, std::vector<float> &out) {
        out.clear();
        out.reserve(count * 2);

        for (std::size_t i = 0; i < count; ++i) {
            const float composite = in[i];

            // Pilot tracking: PI loop, phase detector = pilot * sin(nco).
            // nco_sin/nco_cos must use THIS sample's phase, before it
            // advances below - using next sample's phase here would
            // misalign the demod reference by a full step.
            const float pilot = m_pilot_bpf.push(composite);
            const double nco_sin = std::sin(m_pll_phase);
            const double nco_cos = std::cos(m_pll_phase);
            const double error = pilot * nco_sin;

            // Negative feedback: averaged over a pilot cycle, error ~=
            // +cos^2(phase)*delta for a phase-ahead error delta > 0, so the
            // correction must subtract, not add, or the loop diverges.
            m_pll_freq -= m_pll_ki * error;
            m_pll_phase += m_pll_freq - m_pll_kp * error;
            if (m_pll_phase > M_PI) {
                m_pll_phase -= 2.0 * M_PI;
            } else if (m_pll_phase < -M_PI) {
                m_pll_phase += 2.0 * M_PI;
            }

            // 38 kHz reference via the double-angle identity cos(2*phase) =
            // 2*cos^2(phase) - 1, avoiding a second NCO.
            const double ref38 = 2.0 * nco_cos * nco_cos - 1.0;

            // --- L+R (mono sum): straight lowpass of the composite ---
            const float sum = m_sum_lpf.push(composite);

            // --- L-R: synchronous demod of the 38 kHz DSB-SC, then lowpass.
            // The x2 undoes the demod's inherent 0.5 gain (cos^2 -> 1/2 DC term).
            const float mixed = composite * static_cast<float>(ref38) * 2.0f;
            const float diff = m_diff_lpf.push(mixed);

            out.push_back(sum + diff); // L
            out.push_back(sum - diff); // R
        }
    }

} // namespace dsp

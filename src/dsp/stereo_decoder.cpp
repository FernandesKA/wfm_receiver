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

#include "dsp/fir_design.h"

namespace dsp {

    namespace {
        constexpr std::size_t kPilotBpfTaps = 401;
        constexpr std::size_t kAudioLpfTaps = 129;
    } // namespace

    stereo_decoder::stereo_decoder(const config::stereo_decoder_config &cfg)
        : m_pilot_bpf(design_bandpass(cfg.sample_rate_hz, cfg.pilot_hz - 1000.0, cfg.pilot_hz + 1000.0,
                                       kPilotBpfTaps)),
          m_sum_lpf(design_lowpass(cfg.sample_rate_hz, cfg.audio_bandwidth_hz, kAudioLpfTaps)),
          m_diff_lpf(design_lowpass(cfg.sample_rate_hz, cfg.audio_bandwidth_hz, kAudioLpfTaps)) {
        m_pll_freq = 2.0 * M_PI * cfg.pilot_hz / cfg.sample_rate_hz;
        // Damping ratio ~0.7 at real pilot amplitude (~0.1 post-bandpass).
        // The naive kp=0.01/ki=0.0001 locks fine on a noiseless synthetic
        // tone but gives ζ~0.16 at real amplitude - ~900 Hz p-p wander on
        // real hardware.
        m_pll_kp = 0.00426;
        m_pll_ki = 0.00000091;
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

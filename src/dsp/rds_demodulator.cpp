/**
 * @file rds_demodulator.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Recovers RDS data bits from the WFM composite signal
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/rds_demodulator.h"

#include <cmath>

#include "config/resampler_config.h"
#include "dsp/fir_design.h"

namespace dsp {

    namespace {
        constexpr std::size_t kPilotBpfTaps = 401;
        constexpr std::size_t kSubcarrierBpfTaps = 401;
        constexpr std::size_t kBasebandLpfTaps = 129;
        constexpr double kBasebandCutoffHz = 2400.0;
        constexpr std::size_t kAcquisitionSymbols = 300; // ~250 ms at 1187.5 baud
    } // namespace

    rds_demodulator::rds_demodulator(const config::rds_decoder_config &cfg)
        : m_pilot_bpf(design_bandpass(cfg.sample_rate_hz, cfg.pilot_hz - 1000.0, cfg.pilot_hz + 1000.0, kPilotBpfTaps)),
          m_subcarrier_bpf(design_bandpass(cfg.sample_rate_hz, cfg.subcarrier_hz - 3000.0, cfg.subcarrier_hz + 3000.0,
                                            kSubcarrierBpfTaps)),
          m_baseband_lpf(design_lowpass(cfg.sample_rate_hz, kBasebandCutoffHz, kBasebandLpfTaps)),
          m_symbol_resampler(config::resampler_config{
              cfg.sample_rate_hz, static_cast<uint32_t>(cfg.symbol_rate_hz * static_cast<double>(cfg.oversample))}),
          m_oversample(cfg.oversample) {
        m_pll_freq = 2.0 * M_PI * cfg.pilot_hz / cfg.sample_rate_hz;
        // Same gains as stereo_decoder's pilot PLL: damping ratio ~0.7 at
        // real pilot amplitude (~0.1 post-bandpass). The naive kp=0.01/
        // ki=0.0001 locks fine on a noiseless synthetic tone but gives
        // ζ~0.16 at real amplitude - ~900 Hz p-p wander on real hardware.
        m_pll_kp = 0.00426;
        m_pll_ki = 0.00000091;

        m_correlator_delay.assign(m_oversample, 0.0f);
        m_phase_energy.assign(m_oversample, 0.0);
        m_acquisition_samples = m_oversample * kAcquisitionSymbols;
    }

    void rds_demodulator::process(const float *in, std::size_t count, std::vector<uint8_t> &out) {
        out.clear();

        m_baseband_buffer.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            const float composite = in[i];

            // Pilot PLL - identical structure/gains to stereo_decoder's.
            const float pilot = m_pilot_bpf.push(composite);
            const double nco_sin = std::sin(m_pll_phase);
            const double nco_cos = std::cos(m_pll_phase);
            const double error = pilot * nco_sin;

            m_pll_freq -= m_pll_ki * error;
            m_pll_phase += m_pll_freq - m_pll_kp * error;
            if (m_pll_phase > M_PI) {
                m_pll_phase -= 2.0 * M_PI;
            } else if (m_pll_phase < -M_PI) {
                m_pll_phase += 2.0 * M_PI;
            }

            // 57 kHz reference via the triple-angle identity
            // cos(3*phase) = 4*cos^3(phase) - 3*cos(phase).
            const double ref57 = 4.0 * nco_cos * nco_cos * nco_cos - 3.0 * nco_cos;

            const float subcarrier = m_subcarrier_bpf.push(composite);
            const float mixed = subcarrier * static_cast<float>(ref57) * 2.0f;
            m_baseband_buffer[i] = m_baseband_lpf.push(mixed);
        }

        m_resampled_buffer.clear();
        m_symbol_resampler.process(m_baseband_buffer.data(), m_baseband_buffer.size(), m_resampled_buffer);

        for (float sample : m_resampled_buffer) {
            process_resampled_sample(sample, out);
        }
    }

    void rds_demodulator::process_resampled_sample(float sample, std::vector<uint8_t> &out) {
        // Biphase matched filter: +1 over the chronologically older half of
        // the symbol window, -1 over the newer half.
        m_correlator_delay[m_correlator_pos] = sample;
        m_correlator_pos = (m_correlator_pos + 1) % m_oversample;

        float corr = 0.0f;
        for (std::size_t k = 0; k < m_oversample; ++k) {
            const std::size_t idx = (m_correlator_pos + k) % m_oversample;
            const float kernel = (k < m_oversample / 2) ? 1.0f : -1.0f;
            corr += kernel * m_correlator_delay[idx];
        }

        const std::size_t phase = m_sample_counter % m_oversample;
        ++m_sample_counter;

        if (!m_phase_locked) {
            m_phase_energy[phase] += std::abs(corr);
            if (m_sample_counter >= m_acquisition_samples) {
                m_locked_phase = 0;
                for (std::size_t p = 1; p < m_oversample; ++p) {
                    if (m_phase_energy[p] > m_phase_energy[m_locked_phase]) {
                        m_locked_phase = p;
                    }
                }
                m_phase_locked = true;
            }
            return;
        }

        if (phase != m_locked_phase) {
            return;
        }

        // Differential decode: a constant sign flip from the PLL's +-180 deg
        // lock ambiguity cancels out in the XOR, so corr's sign convention
        // doesn't need to be resolved against the transmitter's.
        const int d_bit = corr > 0.0f ? 1 : 0;
        if (m_have_prev_diff_bit) {
            out.push_back(static_cast<uint8_t>(d_bit ^ m_prev_diff_bit));
        }
        m_prev_diff_bit = d_bit;
        m_have_prev_diff_bit = true;
    }

} // namespace dsp

/**
 * @file quadrature_demodulator.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Quadrature (phase-difference) FM demodulator
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/quadrature_demodulator.h"

#include <cmath>

namespace dsp {

    namespace {
        constexpr float kTwoPi = 6.28318530717958647692f;
    }

    quadrature_demodulator::quadrature_demodulator(const config::fm_demodulator_config &cfg)
        : m_gain(static_cast<float>(cfg.sample_rate_hz) / (kTwoPi * static_cast<float>(cfg.deviation_hz))) {}

    void quadrature_demodulator::process(const std::complex<float> *in, std::size_t count, std::vector<float> &out) {
        out.clear();
        out.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            const std::complex<float> phase_diff = in[i] * std::conj(m_prev_sample);
            out.push_back(std::arg(phase_diff) * m_gain);
            m_prev_sample = in[i];
        }
    }

} // namespace dsp

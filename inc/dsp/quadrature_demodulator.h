/**
 * @file quadrature_demodulator.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Quadrature (phase-difference) FM demodulator
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <complex>
#include <cstddef>
#include <vector>

#include "config/fm_demodulator_config.h"
#include "dsp/fm_demodulator.h"

namespace dsp {

    // Standard "quadrature demod": for each input sample computes
    // arg(z[n] * conj(z[n-1])), the phase advance between consecutive
    // samples, which is proportional to instantaneous frequency. Scaled so
    // that +-deviation_hz maps to a +-1.0 output range.
    // Stateful across calls: carries the last sample of the previous
    // process() call as the phase reference for the next one's first output.
    class quadrature_demodulator final : public fm_demodulator {
    public:
        explicit quadrature_demodulator(const config::fm_demodulator_config &cfg);

        void process(const std::complex<float> *in, std::size_t count, std::vector<float> &out) override;

    private:
        float m_gain;
        std::complex<float> m_prev_sample{0.0f, 0.0f};
    };

} // namespace dsp

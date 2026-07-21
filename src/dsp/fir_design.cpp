/**
 * @file fir_design.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Windowed-sinc FIR filter design (Blackman window)
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/fir_design.h"

#include <cmath>

namespace dsp {

    namespace {

        constexpr float kPi = 3.14159265358979323846f;

        float sinc(float x) {
            if (std::abs(x) < 1e-8f) {
                return 1.0f;
            }
            const float px = kPi * x;
            return std::sin(px) / px;
        }

    } // namespace

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

    std::vector<float> design_bandpass(double fs_hz, double f_lo, double f_hi, std::size_t n_taps) {
        const auto lp_hi = design_lowpass(fs_hz, f_hi, n_taps);
        const auto lp_lo = design_lowpass(fs_hz, f_lo, n_taps);

        std::vector<float> taps(n_taps);
        for (std::size_t i = 0; i < n_taps; ++i) {
            taps[i] = lp_hi[i] - lp_lo[i];
        }
        return taps;
    }

} // namespace dsp

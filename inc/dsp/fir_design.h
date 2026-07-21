/**
 * @file fir_design.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Windowed-sinc FIR filter design (Blackman window)
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <vector>

namespace dsp {

    // Windowed-sinc (Blackman) lowpass, unity DC gain, cutoff fc_hz at fs_hz.
    std::vector<float> design_lowpass(double fs_hz, double fc_hz, std::size_t n_taps);

    // Bandpass [f_lo, f_hi] via spectral subtraction of two lowpass designs.
    std::vector<float> design_bandpass(double fs_hz, double f_lo, double f_hi, std::size_t n_taps);

} // namespace dsp

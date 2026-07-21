/**
 * @file fir_filter.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Minimal non-decimating FIR filter (ring-buffer delay line)
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

    // One sample in, one filtered sample out - reused wherever a stage
    // needs a plain bandpass/lowpass without decimation (pilot/subcarrier
    // bandpass filters, post-mix lowpass filters, ...).
    class fir_filter {
    public:
        explicit fir_filter(std::vector<float> taps);
        float push(float sample);

    private:
        std::vector<float> m_taps;
        std::vector<float> m_delay;
        std::size_t m_pos = 0;
    };

} // namespace dsp

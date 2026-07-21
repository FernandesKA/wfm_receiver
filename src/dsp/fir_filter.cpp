/**
 * @file fir_filter.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Minimal non-decimating FIR filter (ring-buffer delay line)
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/fir_filter.h"

#include <utility>

namespace dsp {

    fir_filter::fir_filter(std::vector<float> taps) : m_taps(std::move(taps)), m_delay(m_taps.size(), 0.0f) {}

    float fir_filter::push(float sample) {
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

} // namespace dsp

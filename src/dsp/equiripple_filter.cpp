/**
 * @file equiripple_filter.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Equiripple (Parks-McClellan) anti-aliasing + decimation filter for the IQ path
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/equiripple_filter.h"

#include "dsp/filters/lowpass_2m_to_200k.hpp"

namespace dsp {

    equiripple_filter::equiripple_filter()
        : m_delay_line(filters::kLowpass2mTo200k.size(), std::complex<float>{0.0f, 0.0f}) {}

    void equiripple_filter::process(const std::complex<float> *in, std::size_t count,
                                     std::vector<std::complex<float>> &out) {
        out.clear();

        const auto &taps = filters::kLowpass2mTo200k;
        const std::size_t n_taps = taps.size();
        const std::size_t decim = filters::kLowpass2mTo200kDecim;

        for (std::size_t i = 0; i < count; ++i) {
            m_delay_line[m_write_pos] = in[i];
            m_write_pos = (m_write_pos + 1) % n_taps;

            if (++m_samples_since_output < decim) {
                continue;
            }
            m_samples_since_output = 0;

            std::complex<float> acc{0.0f, 0.0f};
            for (std::size_t k = 0; k < n_taps; ++k) {
                const std::size_t pos = (m_write_pos + n_taps - 1 - k) % n_taps;
                acc += taps[k] * m_delay_line[pos];
            }
            out.push_back(acc);
        }
    }

} // namespace dsp

/**
 * @file amplitude_monitor.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Accumulates IQ sample amplitude to report a running average
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/amplitude_monitor.h"

#include <cmath>
#include <cstring>

namespace dsp {

    void amplitude_monitor::add_samples(const uint8_t *data, std::size_t length, hardware::iq_sample_format format) {
        double power_sum = 0.0;
        std::size_t pair_count = 0;

        switch (format) {
        case hardware::iq_sample_format::int8_iq: {
            // Interleaved signed 8-bit I/Q samples: I0,Q0,I1,Q1,... (HackRF)
            pair_count = length / 2;
            for (std::size_t i = 0; i < pair_count; ++i) {
                const auto sample_i = static_cast<double>(static_cast<int8_t>(data[2 * i]));
                const auto sample_q = static_cast<double>(static_cast<int8_t>(data[2 * i + 1]));
                power_sum += sample_i * sample_i + sample_q * sample_q; // |I+jQ|^2, monotonic with amplitude
            }
            break;
        }
        case hardware::iq_sample_format::int16_iq: {
            // Interleaved signed 16-bit LE I/Q samples (PlutoSDR/AD9361).
            // memcpy avoids unaligned-access UB from casting the raw pointer.
            pair_count = length / 4;
            for (std::size_t i = 0; i < pair_count; ++i) {
                int16_t raw_i;
                int16_t raw_q;
                std::memcpy(&raw_i, data + 4 * i, sizeof(raw_i));
                std::memcpy(&raw_q, data + 4 * i + 2, sizeof(raw_q));
                const auto sample_i = static_cast<double>(raw_i);
                const auto sample_q = static_cast<double>(raw_q);
                power_sum += sample_i * sample_i + sample_q * sample_q;
            }
            break;
        }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_power_sum += power_sum;
        m_count += pair_count;
    }

    double amplitude_monitor::get_and_reset_average() {
        std::lock_guard<std::mutex> lock(m_mutex);

        const double mean_power = m_count > 0 ? m_power_sum / static_cast<double>(m_count) : 0.0;
        m_power_sum = 0.0;
        m_count = 0;

        return std::sqrt(mean_power);
    }

} // namespace dsp

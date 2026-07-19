/**
 * @file iq_converter.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Converts a source's raw IQ byte buffer into normalized complex samples
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/iq_converter.h"

#include <cstring>

namespace dsp {

    namespace {
        constexpr float kInt8FullScale = 128.0f;   // HackRF: signed 8-bit ADC
        constexpr float kInt16FullScale = 2048.0f; // PlutoSDR/AD9361: 12-bit ADC in an int16 container
    } // namespace

    iq_converter::iq_converter(hardware::iq_sample_format format) : m_format(format) {}

    void iq_converter::process(const uint8_t *in, std::size_t count, std::vector<std::complex<float>> &out) {
        out.clear();

        switch (m_format) {
        case hardware::iq_sample_format::int8_iq: {
            const std::size_t pair_count = count / 2;
            out.reserve(pair_count);
            for (std::size_t i = 0; i < pair_count; ++i) {
                const auto sample_i = static_cast<float>(static_cast<int8_t>(in[2 * i]));
                const auto sample_q = static_cast<float>(static_cast<int8_t>(in[2 * i + 1]));
                out.emplace_back(sample_i / kInt8FullScale, sample_q / kInt8FullScale);
            }
            break;
        }
        case hardware::iq_sample_format::int16_iq: {
            // memcpy avoids unaligned-access UB from casting the raw pointer.
            const std::size_t pair_count = count / 4;
            out.reserve(pair_count);
            for (std::size_t i = 0; i < pair_count; ++i) {
                int16_t raw_i;
                int16_t raw_q;
                std::memcpy(&raw_i, in + 4 * i, sizeof(raw_i));
                std::memcpy(&raw_q, in + 4 * i + 2, sizeof(raw_q));
                out.emplace_back(static_cast<float>(raw_i) / kInt16FullScale, static_cast<float>(raw_q) / kInt16FullScale);
            }
            break;
        }
        }
    }

} // namespace dsp

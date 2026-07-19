/**
 * @file iq_converter.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Converts a source's raw IQ byte buffer into normalized complex samples
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "dsp/block.h"
#include "hw/signal_source.h"

namespace dsp {

    // Converts a source's raw interleaved IQ byte buffer (hardware::iq_sample_format)
    // into normalized std::complex<float> samples, ready for the rest of the
    // DSP chain (e.g. equiripple_filter). Stateless - each sample converts
    // independently of its neighbors - so process() can be called with any
    // chunk size, unlike the filter/resampler stages.
    class iq_converter final : public block<uint8_t, std::complex<float>> {
    public:
        explicit iq_converter(hardware::iq_sample_format format);

        void process(const uint8_t *in, std::size_t count, std::vector<std::complex<float>> &out) override;

    private:
        hardware::iq_sample_format m_format;
    };

} // namespace dsp

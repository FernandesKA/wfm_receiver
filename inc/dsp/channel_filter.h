/**
 * @file channel_filter.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Interface for the channel band-limiting filter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <complex>

#include "dsp/block.h"

namespace dsp {

    // Band-limits a wideband IQ stream down to the wanted channel bandwidth
    // (~200 kHz for broadcast WFM per Carson's rule), rejecting
    // adjacent-channel energy before decimation and demodulation.
    // Sample rate in == sample rate out; only the occupied bandwidth changes.
    class channel_filter : public block<std::complex<float>, std::complex<float>> {
    public:
        ~channel_filter() override = default;
    };

} // namespace dsp

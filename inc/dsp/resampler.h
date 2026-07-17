/**
 * @file resampler.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Interface for a rational sample rate converter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "dsp/block.h"

namespace dsp {

    // Changes the sample rate of a stream by some rational factor.
    // Reused for both the IQ decimation stage (wideband capture rate down
    // toward the channel bandwidth) and the final audio resampling stage
    // (demodulated/de-emphasized signal down to the sound card's rate) -
    // both are the same operation, just on different sample types and rates.
    template <typename Sample>
    class resampler : public block<Sample, Sample> {
    public:
        ~resampler() override = default;
    };

} // namespace dsp

/**
 * @file deemphasis_filter.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Interface for the de-emphasis filter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "dsp/block.h"

namespace dsp {

    // Applies the inverse of the transmitter's pre-emphasis curve (50 us in
    // Europe/Russia, 75 us in the US) to the demodulated baseband signal,
    // restoring a flat audio frequency response.
    class deemphasis_filter : public block<float, float> {
    public:
        ~deemphasis_filter() override = default;
    };

} // namespace dsp

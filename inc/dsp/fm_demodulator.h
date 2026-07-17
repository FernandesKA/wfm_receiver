/**
 * @file fm_demodulator.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Interface for the FM (phase-difference) demodulator
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

    // Turns a complex baseband IQ stream (already channel-filtered and
    // decimated) into a real-valued demodulated signal: mono/composite
    // baseband audio, still carrying any stereo pilot/subcarrier and the
    // transmitter's pre-emphasis.
    class fm_demodulator : public block<std::complex<float>, float> {
    public:
        ~fm_demodulator() override = default;
    };

} // namespace dsp

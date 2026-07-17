/**
 * @file fm_demodulator_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the FM (phase-difference) demodulator
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace config {

    struct fm_demodulator_config {
        uint32_t sample_rate_hz = 200'000;  // rate of the incoming (already decimated) IQ stream
        uint32_t deviation_hz = 75'000;     // max frequency deviation (broadcast WFM), used to scale the output
    };

} // namespace config

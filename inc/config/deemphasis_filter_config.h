/**
 * @file deemphasis_filter_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the de-emphasis filter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace config {

    struct deemphasis_filter_config {
        uint32_t sample_rate_hz = 200'000;  // rate of the signal being filtered
        uint32_t tau_us = 50;               // de-emphasis time constant: 50 us EU/RU, 75 us US
    };

} // namespace config

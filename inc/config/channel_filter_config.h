/**
 * @file channel_filter_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for the channel band-limiting filter
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace config {

    struct channel_filter_config {
        uint32_t sample_rate_hz = 2'000'000;        // input/output sample rate (unchanged by this stage)
        uint32_t bandwidth_hz = 200'000;            // wanted channel bandwidth (Carson's rule for broadcast WFM)
        uint32_t transition_bandwidth_hz = 50'000;  // filter roll-off width
    };

} // namespace config

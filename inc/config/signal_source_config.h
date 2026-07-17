/**
 * @file signal_source_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Configuration for selecting and tuning a signal source (SDR device)
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <string>

namespace config {

    enum class signal_source_type {
        hack_rf,
        pluto_sdr, // not implemented yet
    };

    struct signal_source_config {
        signal_source_type type = signal_source_type::hack_rf;

        uint64_t frequency_hz = 100'000'000;
        uint32_t sample_rate_hz = 2'000'000;

        // HackRF specific gains (ignored by sources that don't support them)
        uint32_t lna_gain_db = 16; // 0-40 dB, step 8
        uint32_t vga_gain_db = 20; // 0-62 dB, step 2
        bool amp_enable = false;

        std::string serial_number; // empty => first device found
    };

} // namespace config

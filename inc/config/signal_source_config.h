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
        pluto_sdr,
    };

    // HackRF's own floor (its minimum supported rate). Fixed, not
    // configurable: the channel filter/decimator downstream are built
    // against this exact rate.
    inline constexpr uint32_t hackrf_sample_rate_hz = 2'000'000;

    // AD9361's floor without a custom FIR filter table loaded. NB: the
    // device reports 2'083'333 as its own minimum but rejects that exact
    // value with EINVAL - 2'083'334 is the real floor.
    inline constexpr uint32_t pluto_sample_rate_hz = 2'083'334;

    // PlutoSDR analog front-end filter bandwidth; must be <= sample rate.
    inline constexpr uint32_t pluto_rf_bandwidth_hz = 2'000'000;

    struct signal_source_config {
        signal_source_type type = signal_source_type::hack_rf;

        uint64_t frequency_hz = 100'000'000;

        // HackRF specific gains (ignored by other sources). No AGC on
        // HackRF, so default to max gain.
        uint32_t lna_gain_db = 40; // 0-40 dB, step 8
        uint32_t vga_gain_db = 62; // 0-62 dB, step 2
        bool amp_enable = true;

        // PlutoSDR specific gain control mode (ignored by other sources):
        // "manual" | "fast_attack" | "slow_attack" | "hybrid". In manual
        // mode rx_gain_db is applied; in the AGC modes rx_gain_db is
        // ignored (the AD9361 picks its own gain).
        std::string gain_control_mode = "manual";

        // PlutoSDR specific manual RX gain (ignored by other sources, and by
        // AGC gain_control_modes). AD9361 gain range is roughly 0-76 dB
        // depending on band; not step-quantized like HackRF's, so it isn't
        // clamped/rounded.
        uint32_t rx_gain_db = 40;

        std::string serial_number; // HackRF only; empty => first device found

        // PlutoSDR context URI, e.g. "usb:5.7.5", "ip:192.168.2.1",
        // "ip:pluto.local". Empty => auto-detect the first available context.
        std::string uri;
    };

} // namespace config

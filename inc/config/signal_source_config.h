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

    // Fixed capture sample rate: HackRF only supports 2-20 MHz, so 2 MHz is
    // both its own hardware floor and already 10x the ~200 kHz a WFM channel
    // needs. Not user-configurable on purpose - the channel filter and
    // decimator downstream are built against this exact rate, so changing it
    // at runtime would mean recomputing their coefficients on the fly.
    inline constexpr uint32_t hackrf_sample_rate_hz = 2'000'000;

    // Fixed capture sample rate for PlutoSDR (AD9361). Measured on real
    // hardware: without a custom FIR filter table loaded, the AD9361's own
    // decimation filters can't go below ~2.083 MHz - so, like HackRF, this is
    // the device's own floor, comfortably above the ~200 kHz a WFM channel
    // needs. Fixed for the same reason: downstream filter/decimator
    // coefficients are computed once, not per run.
    // NB: the device's own "sampling_frequency_available" attribute reports
    // 2'083'333 as the minimum, but on real hardware that exact value is
    // rejected with EINVAL - only 2'083'334 and up are actually accepted.
    inline constexpr uint32_t pluto_sample_rate_hz = 2'083'334;

    // Fixed PlutoSDR analog (RF) front-end filter bandwidth. Must be <= the
    // sample rate; valid range on real hardware is [200'000, 40'000'000] Hz.
    inline constexpr uint32_t pluto_rf_bandwidth_hz = 2'000'000;

    struct signal_source_config {
        signal_source_type type = signal_source_type::hack_rf;

        uint64_t frequency_hz = 100'000'000;

        // HackRF specific gains (ignored by other sources)
        uint32_t lna_gain_db = 16; // 0-40 dB, step 8
        uint32_t vga_gain_db = 20; // 0-62 dB, step 2
        bool amp_enable = false;

        // PlutoSDR specific manual RX gain (ignored by other sources).
        // AD9361 gain range is roughly 0-76 dB depending on band; not
        // step-quantized like HackRF's, so it isn't clamped/rounded.
        uint32_t rx_gain_db = 40;

        std::string serial_number; // HackRF only; empty => first device found

        // PlutoSDR context URI, e.g. "usb:5.7.5", "ip:192.168.2.1",
        // "ip:pluto.local". Empty => auto-detect the first available context.
        std::string uri;
    };

} // namespace config

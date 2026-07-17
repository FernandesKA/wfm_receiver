/**
 * @file json_config_loader.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Loads signal_source_config from JSON
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <string>

#include "config/signal_source_config.h"

namespace config {

    // Parses a signal_source_config from a JSON string. All fields are optional;
    // anything missing keeps its signal_source_config default. Expected fields:
    //   "source"         : "hack_rf" | "pluto_sdr"
    //   "frequency_hz"   : integer
    //   "lna_gain_db"    : integer (HackRF only)
    //   "vga_gain_db"    : integer (HackRF only)
    //   "amp_enable"     : bool    (HackRF only)
    //   "serial_number"  : string
    // Sample rate is not configurable: it's fixed at config::hackrf_sample_rate_hz.
    // Throws std::runtime_error / boost::system::system_error on malformed input.
    signal_source_config parse_signal_source_config(const std::string &json_text);

    // Reads a JSON file and parses it the same way as parse_signal_source_config().
    // Throws std::runtime_error if the file can't be opened.
    signal_source_config load_signal_source_config(const std::string &file_path);

} // namespace config

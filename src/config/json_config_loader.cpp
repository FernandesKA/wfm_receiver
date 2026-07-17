/**
 * @file json_config_loader.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Loads signal_source_config from JSON
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "config/json_config_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <boost/json.hpp>

namespace config {

    namespace {

        signal_source_type parse_source_type(const std::string &name) {
            if (name == "hack_rf") {
                return signal_source_type::hack_rf;
            }
            if (name == "pluto_sdr") {
                return signal_source_type::pluto_sdr;
            }
            throw std::runtime_error("unknown signal source type: " + name);
        }

    } // namespace

    signal_source_config parse_signal_source_config(const std::string &json_text) {
        const boost::json::object obj = boost::json::parse(json_text).as_object();

        signal_source_config cfg; // start from defaults

        if (const auto *v = obj.if_contains("source")) {
            cfg.type = parse_source_type(std::string(v->as_string()));
        }
        if (const auto *v = obj.if_contains("frequency_hz")) {
            cfg.frequency_hz = v->to_number<uint64_t>();
        }
        if (const auto *v = obj.if_contains("lna_gain_db")) {
            cfg.lna_gain_db = v->to_number<uint32_t>();
        }
        if (const auto *v = obj.if_contains("vga_gain_db")) {
            cfg.vga_gain_db = v->to_number<uint32_t>();
        }
        if (const auto *v = obj.if_contains("amp_enable")) {
            cfg.amp_enable = v->as_bool();
        }
        if (const auto *v = obj.if_contains("serial_number")) {
            cfg.serial_number = std::string(v->as_string());
        }

        return cfg;
    }

    signal_source_config load_signal_source_config(const std::string &file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open config file: " + file_path);
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return parse_signal_source_config(buffer.str());
    }

} // namespace config

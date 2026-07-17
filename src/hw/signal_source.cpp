/**
 * @file signal_source.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Factory for creating a signal_source from config
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hw/signal_source.h"

#include "config/json_config_loader.h"
#include "hw/hack_rf.h"

namespace HW {

    std::unique_ptr<signal_source> create_signal_source(const config::signal_source_config &cfg) {
        switch (cfg.type) {
        case config::signal_source_type::hack_rf:
            return std::make_unique<hack_rf>(cfg);
        case config::signal_source_type::pluto_sdr:
        default:
            return nullptr; // not supported yet
        }
    }

    std::unique_ptr<signal_source> create_signal_source_from_file(const std::string &json_config_path) {
        return create_signal_source(config::load_signal_source_config(json_config_path));
    }

} // namespace HW

/**
 * @file signal_source.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Abstract interface for a raw IQ signal source (SDR device)
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "config/signal_source_config.h"

namespace HW {

    // Invoked from the device's capture thread with a raw buffer of IQ samples.
    // The buffer is only valid for the duration of the call.
    using iq_data_callback_t = std::function<void(const uint8_t *data, std::size_t length)>;

    class signal_source {
    public:
        explicit signal_source(const config::signal_source_config &cfg) : m_config(cfg) {}
        virtual ~signal_source() = default;

        signal_source(const signal_source &) = delete;
        signal_source &operator=(const signal_source &) = delete;

        // Opens/configures the device (sample rate, frequency, gains).
        virtual bool open() = 0;

        // Starts asynchronous reception; callback is invoked for every received block.
        virtual bool start(iq_data_callback_t callback) = 0;

        // Stops reception, device stays open.
        virtual void stop() = 0;

        // Stops reception (if needed) and releases the device.
        virtual void close() = 0;

        virtual bool is_running() const = 0;

    protected:
        config::signal_source_config m_config;
    };

    // Factory: creates a concrete signal_source based on cfg.type.
    // Returns nullptr if the requested source type isn't implemented yet.
    std::unique_ptr<signal_source> create_signal_source(const config::signal_source_config &cfg);

    // Convenience factory: loads signal_source_config from a JSON file and
    // creates the corresponding signal_source. Throws if the file is missing
    // or malformed; returns nullptr if the configured source type isn't
    // implemented yet.
    std::unique_ptr<signal_source> create_signal_source_from_file(const std::string &json_config_path);

} // namespace HW

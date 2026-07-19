/**
 * @file hack_rf.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief HackRF One signal source implementation
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <atomic>

#include <hackrf.h>

#include "hw/signal_source.h"

namespace hardware {

    class hack_rf final : public signal_source {
    public:
        explicit hack_rf(const config::signal_source_config &cfg);
        ~hack_rf() override;

        bool open() override;
        bool start(iq_data_callback_t callback) override;
        void stop() override;
        void close() override;
        bool is_running() const override;
        iq_sample_format sample_format() const override {
            return iq_sample_format::int8_iq;
        }

    private:
        static int rx_callback(hackrf_transfer *transfer);

        hackrf_device *m_device = nullptr;
        iq_data_callback_t m_callback;
        std::atomic<bool> m_running{false};
    };

} // namespace hardware

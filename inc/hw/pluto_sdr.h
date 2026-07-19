/**
 * @file pluto_sdr.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief PlutoSDR (ADALM-PLUTO / AD9361) signal source implementation
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <atomic>
#include <thread>

#include <iio.h>

#include "hw/signal_source.h"

namespace hardware {

    class pluto_sdr final : public signal_source {
    public:
        explicit pluto_sdr(const config::signal_source_config &cfg);
        ~pluto_sdr() override;

        bool open() override;
        bool start(iq_data_callback_t callback) override;
        void stop() override;
        void close() override;
        bool is_running() const override;

        iq_sample_format sample_format() const override {
            return iq_sample_format::int16_iq;
        }

    private:
        // Blocking refill loop, run on m_rx_thread: PlutoSDR/libiio has no
        // async callback API like HackRF, so a dedicated thread calls
        // iio_buffer_refill() (blocking) and forwards each refilled buffer
        // to the registered callback.
        void rx_loop();

        iio_context *m_ctx = nullptr;
        iio_device *m_phy = nullptr;
        iio_device *m_rx_dev = nullptr;
        iio_channel *m_rx_i = nullptr;
        iio_channel *m_rx_q = nullptr;
        iio_buffer *m_buffer = nullptr;

        iq_data_callback_t m_callback;
        std::atomic<bool> m_running{false};
        std::thread m_rx_thread;
    };

} // namespace hardware

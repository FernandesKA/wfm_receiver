/**
 * @file pluto_sdr.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief PlutoSDR (ADALM-PLUTO / AD9361) signal source implementation
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hw/pluto_sdr.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

namespace hardware {

    namespace {

        bool check(long result, const char *what) {
            if (result < 0) {
                std::fprintf(stderr, "pluto_sdr: %s failed: %s\n", what, strerror(static_cast<int>(-result)));
                return false;
            }
            return true;
        }

        // Explicit URI (config.uri) connects directly; otherwise scan for
        // whatever IIO context is available (mirrors hack_rf's "first
        // device found" behavior when no serial is given).
        iio_context *create_context(const std::string &uri) {
            if (!uri.empty()) {
                return iio_create_context_from_uri(uri.c_str());
            }

            iio_scan_context *scan_ctx = iio_create_scan_context(nullptr, 0);
            if (scan_ctx == nullptr) {
                return nullptr;
            }

            iio_context_info **info_list = nullptr;
            const ssize_t count = iio_scan_context_get_info_list(scan_ctx, &info_list);

            iio_context *ctx = nullptr;
            if (count > 0) {
                ctx = iio_create_context_from_uri(iio_context_info_get_uri(info_list[0]));
            }

            if (info_list != nullptr) {
                iio_context_info_list_free(info_list);
            }
            iio_scan_context_destroy(scan_ctx);

            return ctx;
        }

    } // namespace

    pluto_sdr::pluto_sdr(const config::signal_source_config &cfg) : signal_source(cfg) {}

    pluto_sdr::~pluto_sdr() {
        close();
    }

    bool pluto_sdr::open() {
        if (m_ctx != nullptr) {
            return true;
        }

        m_ctx = create_context(m_config.uri);
        if (m_ctx == nullptr) {
            std::fprintf(stderr, "pluto_sdr: failed to create iio context\n");
            return false;
        }

        m_phy = iio_context_find_device(m_ctx, "ad9361-phy");
        m_rx_dev = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
        if (m_phy == nullptr || m_rx_dev == nullptr) {
            std::fprintf(stderr, "pluto_sdr: ad9361-phy/cf-ad9361-lpc device not found\n");
            iio_context_destroy(m_ctx);
            m_ctx = nullptr;
            return false;
        }

        iio_channel *rx_cfg_chn = iio_device_find_channel(m_phy, "voltage0", false);
        iio_channel *lo_chn = iio_device_find_channel(m_phy, "altvoltage0", true);
        if (rx_cfg_chn == nullptr || lo_chn == nullptr) {
            std::fprintf(stderr, "pluto_sdr: ad9361-phy RX/LO channel not found\n");
            iio_context_destroy(m_ctx);
            m_ctx = nullptr;
            return false;
        }

        bool ok = true;
        if (!check(iio_channel_attr_write(rx_cfg_chn, "rf_port_select", "A_BALANCED"), "rf_port_select")) {
            ok = false;
        }
        if (!check(iio_channel_attr_write_longlong(rx_cfg_chn, "rf_bandwidth", config::pluto_rf_bandwidth_hz),
                   "rf_bandwidth")) {
            ok = false;
        }
        if (!check(iio_channel_attr_write_longlong(rx_cfg_chn, "sampling_frequency", config::pluto_sample_rate_hz),
                   "sampling_frequency")) {
            ok = false;
        }
        if (!check(iio_channel_attr_write(rx_cfg_chn, "gain_control_mode", m_config.gain_control_mode.c_str()),
                   "gain_control_mode")) {
            ok = false;
        }
        if (m_config.gain_control_mode == "manual") {
            if (!check(iio_channel_attr_write_double(rx_cfg_chn, "hardwaregain",
                                                      static_cast<double>(m_config.rx_gain_db)),
                       "hardwaregain")) {
                ok = false;
            }
        }
        if (!check(iio_channel_attr_write_longlong(lo_chn, "frequency", static_cast<long long>(m_config.frequency_hz)),
                   "RX LO frequency")) {
            ok = false;
        }

        if (!ok) {
            iio_context_destroy(m_ctx);
            m_ctx = nullptr;
            return false;
        }

        m_rx_i = iio_device_find_channel(m_rx_dev, "voltage0", false);
        m_rx_q = iio_device_find_channel(m_rx_dev, "voltage1", false);
        if (m_rx_i == nullptr || m_rx_q == nullptr) {
            std::fprintf(stderr, "pluto_sdr: cf-ad9361-lpc RX channels not found\n");
            iio_context_destroy(m_ctx);
            m_ctx = nullptr;
            return false;
        }

        iio_channel_enable(m_rx_i);
        iio_channel_enable(m_rx_q);

        return true;
    }

    bool pluto_sdr::start(iq_data_callback_t callback) {
        if (m_rx_dev == nullptr || m_running.load()) {
            return false;
        }

        constexpr std::size_t buffer_size_samples = 1 << 15; // ~16k I/Q pairs per refill
        m_buffer = iio_device_create_buffer(m_rx_dev, buffer_size_samples, false);
        if (m_buffer == nullptr) {
            std::fprintf(stderr, "pluto_sdr: iio_device_create_buffer failed: %s\n", strerror(errno));
            return false;
        }

        m_callback = std::move(callback);
        m_running.store(true);
        m_rx_thread = std::thread(&pluto_sdr::rx_loop, this);

        return true;
    }

    void pluto_sdr::stop() {
        if (!m_running.exchange(false)) {
            return;
        }

        if (m_buffer != nullptr) {
            iio_buffer_cancel(m_buffer); // unblocks a pending iio_buffer_refill()
        }

        if (m_rx_thread.joinable()) {
            m_rx_thread.join();
        }
    }

    void pluto_sdr::close() {
        stop();

        if (m_buffer != nullptr) {
            iio_buffer_destroy(m_buffer);
            m_buffer = nullptr;
        }

        if (m_ctx != nullptr) {
            iio_context_destroy(m_ctx);
            m_ctx = nullptr;
            m_phy = nullptr;
            m_rx_dev = nullptr;
            m_rx_i = nullptr;
            m_rx_q = nullptr;
        }
    }

    bool pluto_sdr::is_running() const {
        return m_running.load();
    }

    void pluto_sdr::rx_loop() {
        while (m_running.load()) {
            const ssize_t nbytes = iio_buffer_refill(m_buffer);
            if (nbytes < 0) {
                if (m_running.load()) {
                    std::fprintf(stderr, "pluto_sdr: iio_buffer_refill failed: %s\n", strerror(static_cast<int>(-nbytes)));
                }
                break;
            }

            if (m_callback) {
                const auto *data = static_cast<const uint8_t *>(iio_buffer_start(m_buffer));
                m_callback(data, static_cast<std::size_t>(nbytes));
            }
        }

        m_running.store(false);
    }

} // namespace hardware

/**
 * @file hack_rf.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief HackRF One signal source implementation
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hw/hack_rf.h"

#include <cstdio>
#include <utility>

namespace HW {

    namespace {
        // hackrf_init()/hackrf_exit() are process-global, so only call them
        // once even if multiple hack_rf instances are created.
        std::atomic<int> g_hackrf_ref_count{0};

        bool check(int result, const char *what) {
            if (result != HACKRF_SUCCESS) {
                std::fprintf(stderr, "hackrf: %s failed: %s\n", what,
                             hackrf_error_name(static_cast<hackrf_error>(result)));
                return false;
            }
            return true;
        }
    }

    hack_rf::hack_rf(const config::signal_source_config &cfg) : signal_source(cfg) {}

    hack_rf::~hack_rf() {
        close();
    }

    bool hack_rf::open() {
        if (m_device != nullptr) {
            return true;
        }

        if (g_hackrf_ref_count.fetch_add(1) == 0 && hackrf_init() != HACKRF_SUCCESS) {
            g_hackrf_ref_count.fetch_sub(1);
            return false;
        }

        const char *serial = m_config.serial_number.empty() ? nullptr : m_config.serial_number.c_str();
        if (hackrf_open_by_serial(serial, &m_device) != HACKRF_SUCCESS) {
            m_device = nullptr;
            if (g_hackrf_ref_count.fetch_sub(1) == 1) {
                hackrf_exit();
            }
            return false;
        }

        bool ok = true;
        if (!check(hackrf_set_sample_rate(m_device, m_config.sample_rate_hz), "hackrf_set_sample_rate")) {
            ok = false;
        }
        if (!check(hackrf_set_freq(m_device, m_config.frequency_hz), "hackrf_set_freq")) {
            ok = false;
        }
        if (!check(hackrf_set_lna_gain(m_device, m_config.lna_gain_db), "hackrf_set_lna_gain")) {
            ok = false;
        }
        if (!check(hackrf_set_vga_gain(m_device, m_config.vga_gain_db), "hackrf_set_vga_gain")) {
            ok = false;
        }
        if (!check(hackrf_set_amp_enable(m_device, m_config.amp_enable ? 1 : 0), "hackrf_set_amp_enable")) {
            ok = false;
        }

        if (!ok) {
            hackrf_close(m_device);
            m_device = nullptr;
            if (g_hackrf_ref_count.fetch_sub(1) == 1) {
                hackrf_exit();
            }
            return false;
        }

        return true;
    }

    bool hack_rf::start(iq_data_callback_t callback) {
        if (m_device == nullptr || m_running.load()) {
            return false;
        }

        m_callback = std::move(callback);

        if (hackrf_start_rx(m_device, &hack_rf::rx_callback, this) != HACKRF_SUCCESS) {
            return false;
        }

        m_running.store(true);
        return true;
    }

    void hack_rf::stop() {
        if (m_device != nullptr && m_running.load()) {
            hackrf_stop_rx(m_device);
        }
        m_running.store(false);
    }

    void hack_rf::close() {
        stop();

        if (m_device != nullptr) {
            hackrf_close(m_device);
            m_device = nullptr;

            if (g_hackrf_ref_count.fetch_sub(1) == 1) {
                hackrf_exit();
            }
        }
    }

    bool hack_rf::is_running() const {
        return m_running.load();
    }

    int hack_rf::rx_callback(hackrf_transfer *transfer) {
        auto *self = static_cast<hack_rf *>(transfer->rx_ctx);
        if (self != nullptr && self->m_callback) {
            self->m_callback(transfer->buffer, static_cast<std::size_t>(transfer->valid_length));
        }
        return 0;
    }

} // namespace HW

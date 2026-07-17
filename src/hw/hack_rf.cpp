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

namespace hardware {

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

        // Clamps value to [0, max_value] and rounds down to the nearest valid step,
        // warning on stderr if the requested value had to be adjusted.
        uint32_t clamp_gain(uint32_t value, uint32_t max_value, uint32_t step, const char *what) {
            uint32_t clamped = value > max_value ? max_value : value;
            clamped -= clamped % step;

            if (clamped != value) {
                std::fprintf(stderr, "hackrf: %s %u dB out of range/step (0-%u, step %u), using %u dB\n", what,
                             value, max_value, step, clamped);
            }

            return clamped;
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

        // The MAX2837 baseband filter doesn't track the sample rate on its own;
        // hackrf_set_sample_rate() only picks a firmware-default bandwidth (<=0.75*Fs).
        // Set it explicitly so it always matches the configured sample rate.
        const uint32_t baseband_filter_bw_hz =
            hackrf_compute_baseband_filter_bw_round_down_lt(m_config.sample_rate_hz);
        if (!check(hackrf_set_baseband_filter_bandwidth(m_device, baseband_filter_bw_hz),
                   "hackrf_set_baseband_filter_bandwidth")) {
            ok = false;
        }

        if (!check(hackrf_set_freq(m_device, m_config.frequency_hz), "hackrf_set_freq")) {
            ok = false;
        }

        const uint32_t lna_gain_db = clamp_gain(m_config.lna_gain_db, 40, 8, "hackrf_set_lna_gain");
        if (!check(hackrf_set_lna_gain(m_device, lna_gain_db), "hackrf_set_lna_gain")) {
            ok = false;
        }

        const uint32_t vga_gain_db = clamp_gain(m_config.vga_gain_db, 62, 2, "hackrf_set_vga_gain");
        if (!check(hackrf_set_vga_gain(m_device, vga_gain_db), "hackrf_set_vga_gain")) {
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

} // namespace hardware

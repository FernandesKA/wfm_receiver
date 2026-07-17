/**
 * @file amplitude_monitor.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Accumulates IQ sample amplitude to report a running average
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace dsp {

    // Consumes raw interleaved 8-bit IQ samples (as produced by hardware::signal_source)
    // and tracks the RMS sample amplitude since the last reset.
    // add_samples() accumulates |I+jQ|^2 (power, no sqrt needed per sample);
    // get_and_reset_average() takes a single sqrt of the mean power to report
    // the RMS amplitude, so the expensive sqrt only happens once per report,
    // not once per sample.
    // add_samples() is meant to be called from the capture thread,
    // get_and_reset_average() from a separate reporting thread.
    class amplitude_monitor {
    public:
        void add_samples(const uint8_t *data, std::size_t length);

        // Returns the RMS amplitude accumulated since the previous call,
        // then resets the accumulator. Returns 0 if no samples were seen.
        double get_and_reset_average();

    private:
        std::mutex m_mutex;
        double m_power_sum = 0.0;
        uint64_t m_count = 0;
    };

} // namespace dsp

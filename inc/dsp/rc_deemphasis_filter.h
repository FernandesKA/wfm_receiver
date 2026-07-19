/**
 * @file rc_deemphasis_filter.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Single-pole RC de-emphasis filter
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <vector>

#include "config/deemphasis_filter_config.h"
#include "dsp/deemphasis_filter.h"

namespace dsp {

    // Discretized single-pole RC low-pass (leaky integrator):
    //   y[n] = y[n-1] + alpha * (x[n] - y[n-1]),  alpha = dt / (tau + dt)
    // This is the standard de-emphasis implementation used across SDR
    // software (rtl_fm, GNU Radio's fm_deemph, etc.) - equivalent to the
    // analog RC network the transmitter's pre-emphasis is meant to be undone
    // by. Stateful across calls: carries the last output sample as y[-1] for
    // the next process() call.
    class rc_deemphasis_filter final : public deemphasis_filter {
    public:
        explicit rc_deemphasis_filter(const config::deemphasis_filter_config &cfg);

        void process(const float *in, std::size_t count, std::vector<float> &out) override;

    private:
        float m_alpha;
        float m_prev_output = 0.0f;
    };

} // namespace dsp

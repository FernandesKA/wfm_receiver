/**
 * @file rc_deemphasis_filter.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Single-pole RC de-emphasis filter
 * @version 0.1
 * @date 2026-07-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/rc_deemphasis_filter.h"

namespace dsp {

    rc_deemphasis_filter::rc_deemphasis_filter(const config::deemphasis_filter_config &cfg) {
        const float dt = 1.0f / static_cast<float>(cfg.sample_rate_hz);
        const float tau = static_cast<float>(cfg.tau_us) * 1e-6f;
        m_alpha = dt / (tau + dt);
    }

    void rc_deemphasis_filter::process(const float *in, std::size_t count, std::vector<float> &out) {
        out.clear();
        out.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            m_prev_output += m_alpha * (in[i] - m_prev_output);
            out.push_back(m_prev_output);
        }
    }

} // namespace dsp

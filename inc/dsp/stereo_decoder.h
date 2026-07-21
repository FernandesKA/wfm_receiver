/**
 * @file stereo_decoder.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief WFM stereo (pilot-locked DSB-SC) decoder
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <vector>

#include "config/stereo_decoder_config.h"
#include "dsp/block.h"
#include "dsp/fir_filter.h"

namespace dsp {

    // Decodes a WFM composite signal - L+R at 0-15 kHz, 19 kHz pilot, L-R on
    // a 38 kHz DSB-SC subcarrier (23-53 kHz) - into interleaved L,R,L,R,...
    // at the same sample rate.
    //
    // Must run right after the FM discriminator, before audio_resampler
    // (whose own cutoff already discards everything above ~21.6 kHz).
    //
    // A 2nd-order PLL locks onto the pilot to regenerate a phase-coherent
    // 38 kHz reference; a free-running oscillator would drift in phase and
    // degrade the stereo separation over time.
    class stereo_decoder final : public block<float, float> {
    public:
        explicit stereo_decoder(const config::stereo_decoder_config &cfg);

        // out is interleaved L,R,L,R,... (2x the length of the mono input).
        void process(const float *in, std::size_t count, std::vector<float> &out) override;

    private:
        fir_filter m_pilot_bpf;
        fir_filter m_sum_lpf;  // L+R
        fir_filter m_diff_lpf; // L-R, applied after synchronous demod

        // Pilot PLL state (2nd-order PI loop tracking the 19 kHz pilot).
        double m_pll_phase = 0.0;
        double m_pll_freq;
        double m_pll_kp;
        double m_pll_ki;
    };

} // namespace dsp

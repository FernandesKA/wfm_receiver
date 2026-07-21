/**
 * @file rds_demodulator.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Recovers RDS data bits from the WFM composite signal
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "config/rds_decoder_config.h"
#include "dsp/audio_resampler.h"
#include "dsp/block.h"
#include "dsp/fir_filter.h"

namespace dsp {

    // RDS rides a 57 kHz subcarrier (3x the pilot); symbols are biphase-coded
    // at 1187.5 baud and differentially encoded, so the coherent demod's
    // +-180 deg lock ambiguity cancels out in the XOR decode. Own PLL locks
    // the pilot independently of stereo_decoder's; the 57 kHz reference uses
    // the triple-angle identity cos(3*phase) = 4*cos^3(phase) - 3*cos(phase)
    // to avoid a second NCO. Symbol timing is phase-locked to the same
    // oscillator as the pilot, so acquisition just needs to find one fixed
    // sample-phase offset rather than continuously track it.
    class rds_demodulator final : public block<float, uint8_t> {
    public:
        explicit rds_demodulator(const config::rds_decoder_config &cfg);

        void process(const float *in, std::size_t count, std::vector<uint8_t> &out) override;

    private:
        void process_resampled_sample(float sample, std::vector<uint8_t> &out);

        fir_filter m_pilot_bpf;
        fir_filter m_subcarrier_bpf;
        fir_filter m_baseband_lpf;
        audio_resampler m_symbol_resampler;

        // Pilot PLL state, same design as stereo_decoder's.
        double m_pll_phase = 0.0;
        double m_pll_freq;
        double m_pll_kp;
        double m_pll_ki;

        // Biphase matched filter, ring buffer of length m_oversample.
        std::vector<float> m_correlator_delay;
        std::size_t m_correlator_pos = 0;
        std::size_t m_oversample;

        // Which of the m_oversample resampled positions per symbol is the
        // correct decision point.
        std::size_t m_sample_counter = 0;
        bool m_phase_locked = false;
        std::size_t m_locked_phase = 0;
        std::vector<double> m_phase_energy;
        std::size_t m_acquisition_samples;

        int m_prev_diff_bit = 0;
        bool m_have_prev_diff_bit = false;

        // Scratch buffers reused across process() calls.
        std::vector<float> m_baseband_buffer;
        std::vector<float> m_resampled_buffer;
    };

} // namespace dsp

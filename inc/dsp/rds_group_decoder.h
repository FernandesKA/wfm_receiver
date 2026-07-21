/**
 * @file rds_group_decoder.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief RDS block/group protocol decoder (PI, PTY, PS, RadioText)
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dsp {

    // Consumes the bit stream from rds_demodulator and extracts PI, PTY,
    // Program Service (PS) and RadioText (RT). Not a dsp::block: a stateful
    // protocol parser, not a fixed-shape stream transform. Block sync slides
    // a 26-bit window until its CRC syndrome matches one of the 5 offset
    // words (A/B/C/C'/D), then reads fixed 26-bit steps cycling
    // A->B->C(or C')->D, dropping back to the bit-by-bit search on mismatch.
    class rds_group_decoder {
    public:
        rds_group_decoder();

        // Feed newly recovered bits (each element 0 or 1).
        void push_bits(const uint8_t *bits, std::size_t count);

        bool has_pi() const {
            return m_has_pi;
        }
        uint16_t pi() const {
            return m_pi;
        }

        bool has_pty() const {
            return m_has_pty;
        }
        uint8_t pty() const {
            return m_pty;
        }

        // Program Service name, 8 chars, space-padded for segments not yet received.
        const std::string &program_service() const {
            return m_ps;
        }

        // RadioText, up to 64 chars, space-padded for segments not yet received.
        const std::string &radio_text() const {
            return m_rt;
        }

        // True if program_service()/radio_text() changed since the last
        // call to the respective consume_*_changed(); clears the flag.
        bool consume_ps_changed();
        bool consume_rt_changed();

    private:
        void push_bit(uint8_t bit);
        int find_offset_match(uint32_t window26, bool &is_c_prime) const;
        void store_block(int type, uint32_t window26, bool is_c_prime);
        void handle_group();
        void update_rt_char(std::size_t index, char c);
        void update_ps_char(std::size_t index, char c);

        uint32_t m_shift_reg = 0;
        std::size_t m_bits_since_check = 0;
        bool m_synced = false;
        int m_expected_type = 0; // 0=A, 1=B, 2=C/C', 3=D

        uint16_t m_block_a = 0;
        uint16_t m_block_b = 0;
        uint16_t m_block_c = 0;
        uint16_t m_block_d = 0;
        bool m_block_c_is_pi = false;

        bool m_has_pi = false;
        uint16_t m_pi = 0;
        // Debounced like PS/RT: block A repeats every group, so PI commits
        // only after two consecutive matching reads (rejects a lucky false
        // block-sync lock on noise).
        bool m_has_pi_pending = false;
        uint16_t m_pi_pending = 0;
        bool m_has_pty = false;
        uint8_t m_pty = 0;

        std::string m_ps;
        std::string m_rt;
        // Last raw (unconfirmed) char per index; committed only once read
        // identically twice in a row, to reject single corrupted blocks.
        std::string m_ps_pending;
        std::string m_rt_pending;
        bool m_rt_ab_flag_valid = false;
        bool m_rt_ab_flag = false;
        bool m_ps_changed = false;
        bool m_rt_changed = false;
    };

} // namespace dsp

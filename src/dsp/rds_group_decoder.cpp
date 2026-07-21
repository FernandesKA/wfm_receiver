/**
 * @file rds_group_decoder.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief RDS block/group protocol decoder (PI, PTY, PS, RadioText)
 * @version 0.1
 * @date 2026-07-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dsp/rds_group_decoder.h"

namespace dsp {

    namespace {
        // g(x) = x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + 1, as an 11-bit value
        // (bit 10 is the degree-10 leading term).
        constexpr uint32_t kGeneratorPoly = 0x5B9;

        constexpr uint16_t kOffsetA = 0x0FC;
        constexpr uint16_t kOffsetB = 0x198;
        constexpr uint16_t kOffsetC = 0x168;
        constexpr uint16_t kOffsetCPrime = 0x350;
        constexpr uint16_t kOffsetD = 0x1B4;

        constexpr uint32_t kWindowMask = 0x3FFFFFFu; // 26 bits

        uint16_t compute_crc(uint16_t data16) {
            uint32_t reg = static_cast<uint32_t>(data16) << 10;
            for (int i = 25; i >= 10; --i) {
                if (reg & (1u << i)) {
                    reg ^= (kGeneratorPoly << (i - 10));
                }
            }
            return static_cast<uint16_t>(reg & 0x3FFu);
        }
    } // namespace

    rds_group_decoder::rds_group_decoder() {
        m_ps.assign(8, ' ');
        m_rt.assign(64, ' ');
        m_ps_pending.assign(8, ' ');
        m_rt_pending.assign(64, ' ');
    }

    void rds_group_decoder::push_bits(const uint8_t *bits, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            push_bit(bits[i]);
        }
    }

    bool rds_group_decoder::consume_ps_changed() {
        const bool changed = m_ps_changed;
        m_ps_changed = false;
        return changed;
    }

    bool rds_group_decoder::consume_rt_changed() {
        const bool changed = m_rt_changed;
        m_rt_changed = false;
        return changed;
    }

    int rds_group_decoder::find_offset_match(uint32_t window26, bool &is_c_prime) const {
        const uint16_t data16 = static_cast<uint16_t>((window26 >> 10) & 0xFFFFu);
        const uint16_t checkword = static_cast<uint16_t>(window26 & 0x3FFu);
        const uint16_t syndrome = static_cast<uint16_t>(compute_crc(data16) ^ checkword);

        is_c_prime = false;
        if (syndrome == kOffsetA) return 0;
        if (syndrome == kOffsetB) return 1;
        if (syndrome == kOffsetC) return 2;
        if (syndrome == kOffsetCPrime) {
            is_c_prime = true;
            return 2;
        }
        if (syndrome == kOffsetD) return 3;
        return -1;
    }

    void rds_group_decoder::push_bit(uint8_t bit) {
        m_shift_reg = ((m_shift_reg << 1) | (bit & 1u)) & kWindowMask;
        ++m_bits_since_check;

        if (!m_synced) {
            bool is_c_prime = false;
            const int type = find_offset_match(m_shift_reg, is_c_prime);
            if (type >= 0) {
                m_synced = true;
                m_bits_since_check = 0;
                store_block(type, m_shift_reg, is_c_prime);
                m_expected_type = (type + 1) % 4;
            }
            return;
        }

        if (m_bits_since_check < 26) {
            return;
        }
        m_bits_since_check = 0;

        bool is_c_prime = false;
        const int type = find_offset_match(m_shift_reg, is_c_prime);
        if (type != m_expected_type) {
            // Lost sync; resume the bit-by-bit search from scratch.
            m_synced = false;
            return;
        }

        store_block(type, m_shift_reg, is_c_prime);
        m_expected_type = (m_expected_type + 1) % 4;
    }

    void rds_group_decoder::store_block(int type, uint32_t window26, bool is_c_prime) {
        const uint16_t data16 = static_cast<uint16_t>((window26 >> 10) & 0xFFFFu);
        switch (type) {
        case 0:
            m_block_a = data16;
            if (m_has_pi_pending && m_pi_pending == data16) {
                if (!m_has_pi || m_pi != data16) {
                    m_pi = data16;
                    m_has_pi = true;
                }
            } else {
                m_pi_pending = data16;
                m_has_pi_pending = true;
            }
            break;
        case 1:
            m_block_b = data16;
            break;
        case 2:
            m_block_c = data16;
            m_block_c_is_pi = is_c_prime;
            break;
        case 3:
            m_block_d = data16;
            handle_group();
            break;
        default:
            break;
        }
    }

    void rds_group_decoder::update_ps_char(std::size_t index, char c) {
        if (index >= m_ps.size()) {
            return;
        }
        if (m_ps_pending[index] == c) {
            if (m_ps[index] != c) {
                m_ps[index] = c;
                m_ps_changed = true;
            }
        } else {
            m_ps_pending[index] = c;
        }
    }

    void rds_group_decoder::update_rt_char(std::size_t index, char c) {
        if (index >= m_rt.size()) {
            return;
        }
        if (m_rt_pending[index] == c) {
            if (m_rt[index] != c) {
                m_rt[index] = c;
                m_rt_changed = true;
            }
        } else {
            m_rt_pending[index] = c;
        }
    }

    void rds_group_decoder::handle_group() {
        const int group_type = (m_block_b >> 12) & 0xF;
        const bool version_b = ((m_block_b >> 11) & 0x1) != 0;

        m_pty = static_cast<uint8_t>((m_block_b >> 5) & 0x1F);
        m_has_pty = true;

        if (group_type == 0) {
            const std::size_t segment = m_block_b & 0x3u;
            const char c0 = static_cast<char>((m_block_d >> 8) & 0xFFu);
            const char c1 = static_cast<char>(m_block_d & 0xFFu);
            update_ps_char(segment * 2, c0);
            update_ps_char(segment * 2 + 1, c1);
        } else if (group_type == 2) {
            const bool ab_flag = ((m_block_b >> 4) & 0x1) != 0;
            if (m_rt_ab_flag_valid && ab_flag != m_rt_ab_flag) {
                // Transmitter started a new RadioText message; clear stale
                // content and any pending (not-yet-confirmed) characters.
                m_rt.assign(64, ' ');
                m_rt_pending.assign(64, ' ');
                m_rt_changed = true;
            }
            m_rt_ab_flag = ab_flag;
            m_rt_ab_flag_valid = true;

            const std::size_t segment = m_block_b & 0xFu;
            if (!version_b) {
                // Group 2A: 4 chars/group from blocks C+D.
                const char c0 = static_cast<char>((m_block_c >> 8) & 0xFFu);
                const char c1 = static_cast<char>(m_block_c & 0xFFu);
                const char c2 = static_cast<char>((m_block_d >> 8) & 0xFFu);
                const char c3 = static_cast<char>(m_block_d & 0xFFu);
                const std::size_t base = segment * 4;
                update_rt_char(base + 0, c0);
                update_rt_char(base + 1, c1);
                update_rt_char(base + 2, c2);
                update_rt_char(base + 3, c3);
            } else {
                // Group 2B: 2 chars/group from block D only; block C repeats PI.
                const char c0 = static_cast<char>((m_block_d >> 8) & 0xFFu);
                const char c1 = static_cast<char>(m_block_d & 0xFFu);
                const std::size_t base = segment * 2;
                update_rt_char(base + 0, c0);
                update_rt_char(base + 1, c1);
            }
        }
    }

} // namespace dsp

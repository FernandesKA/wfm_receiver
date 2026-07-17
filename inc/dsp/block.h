/**
 * @file block.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Common interface for a stream-processing DSP stage
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <vector>

namespace dsp {

    // A single stage in the receive chain: consumes `count` input samples
    // and appends the resulting output samples to `out` (cleared first).
    // Implementations may be stateful across calls (filter history,
    // resampler phase, demodulator phase reference, ...) so callers can feed
    // the stream in arbitrarily sized chunks and still get correct output.
    template <typename In, typename Out>
    class block {
    public:
        virtual ~block() = default;

        virtual void process(const In *in, std::size_t count, std::vector<Out> &out) = 0;
    };

} // namespace dsp

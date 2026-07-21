/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Demo: receive IQ data from the configured signal source, run it
 *        through the WFM receive chain, and play the result as audio.
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <complex>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "config/audio_sink_config.h"
#include "config/deemphasis_filter_config.h"
#include "config/fm_demodulator_config.h"
#include "config/json_config_loader.h"
#include "config/rds_decoder_config.h"
#include "config/resampler_config.h"
#include "config/stereo_decoder_config.h"
#include "dsp/audio_resampler.h"
#include "dsp/equiripple_filter.h"
#include "dsp/filters/lowpass_2m_to_200k.hpp"
#include "dsp/iq_converter.h"
#include "dsp/quadrature_demodulator.h"
#include "dsp/rc_deemphasis_filter.h"
#include "dsp/rds_demodulator.h"
#include "dsp/rds_group_decoder.h"
#include "dsp/stereo_decoder.h"
#include "hw/audio_sink.h"
#include "hw/signal_source.h"

namespace po = boost::program_options;

namespace {

    std::atomic<bool> g_running{true};

    void handle_sigint(int) {
        g_running.store(false);
    }

    std::string source_type_name(config::signal_source_type type) {
        switch (type) {
        case config::signal_source_type::hack_rf:
            return "HackRF";
        case config::signal_source_type::pluto_sdr:
            return "PlutoSDR";
        }
        return "unknown";
    }

    void print_config(const config::signal_source_config &cfg) {
        std::cout << std::fixed << std::setprecision(3) << "Signal source parameters:\n"
                   << "  source        : " << source_type_name(cfg.type) << "\n"
                   << "  frequency     : " << cfg.frequency_hz / 1'000'000.0 << " MHz\n";

        if (cfg.type == config::signal_source_type::pluto_sdr) {
            std::cout << "  sample rate   : " << config::pluto_sample_rate_hz / 1'000'000.0 << " MHz (fixed)\n"
                       << "  gain mode     : " << cfg.gain_control_mode << "\n"
                       << "  RX gain       : " << cfg.rx_gain_db << " dB"
                       << (cfg.gain_control_mode == "manual" ? "" : " (ignored, AGC active)") << "\n"
                       << "  uri           : " << (cfg.uri.empty() ? "(auto)" : cfg.uri) << std::endl;
        } else {
            std::cout << "  sample rate   : " << config::hackrf_sample_rate_hz / 1'000'000.0 << " MHz (fixed)\n"
                       << "  LNA gain      : " << cfg.lna_gain_db << " dB\n"
                       << "  VGA gain      : " << cfg.vga_gain_db << " dB\n"
                       << "  amp enable    : " << (cfg.amp_enable ? "yes" : "no") << "\n"
                       << "  serial number : " << (cfg.serial_number.empty() ? "(auto)" : cfg.serial_number)
                       << std::endl;
        }

        std::cout.unsetf(std::ios::fixed);
    }

    // Capture rate actually being fed into the channel filter/decimator.
    // NB: equiripple_filter's coefficients are designed for exactly
    // hackrf_sample_rate_hz (2 MHz); PlutoSDR's fixed rate is ~4% off
    // (2.083334 MHz), so its passband/stopband edges shift by that same
    // ~4% for the Pluto path. Not corrected here - a known simplification.
    uint32_t capture_rate_hz(const config::signal_source_config &cfg) {
        return cfg.type == config::signal_source_type::pluto_sdr ? config::pluto_sample_rate_hz
                                                                   : config::hackrf_sample_rate_hz;
    }

    // Thread-safe bounded buffer decoupling the capture thread (producer,
    // must never block) from the playback thread (consumer, blocking ALSA
    // writes) - a slow write here must not stall hardware capture upstream.
    class audio_queue {
    public:
        explicit audio_queue(std::size_t max_samples) : m_max_samples(max_samples) {}

        void push(const float *data, std::size_t count) {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_buffer.size() + count > m_max_samples) {
                // Playback fell behind: drop the oldest buffered samples
                // rather than growing unbounded or blocking the capture
                // thread to wait for the consumer.
                const std::size_t to_drop = std::min(m_buffer.size(), m_buffer.size() + count - m_max_samples);
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(to_drop));
            }
            m_buffer.insert(m_buffer.end(), data, data + count);

            lock.unlock();
            m_cv.notify_one();
        }

        // Blocks until samples are available or stop() is called. Returns
        // false (with `out` left untouched) once stopped and drained - the
        // signal for the playback thread to exit.
        bool pop_all(std::vector<float> &out) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_buffer.empty() || m_stopped; });

            if (m_buffer.empty()) {
                return false;
            }

            out.assign(m_buffer.begin(), m_buffer.end());
            m_buffer.clear();
            return true;
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopped = true;
            }
            m_cv.notify_all();
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::deque<float> m_buffer;
        std::size_t m_max_samples;
        bool m_stopped = false;
    };

} // namespace

int main(int argc, char **argv) {
    po::options_description desc("wfm_receiver options");
    // clang-format off
    desc.add_options()
        ("help,h", "print this help message")
        ("config,c", po::value<std::string>()->default_value("configs/pluto_signal_source.json"),
            "path to JSON signal source config")
        ("frequency,f", po::value<uint64_t>(), "center frequency in Hz (overrides config)")
        ("lna-gain", po::value<uint32_t>(), "HackRF LNA gain in dB, 0-40 step 8 (overrides config)")
        ("vga-gain", po::value<uint32_t>(), "HackRF VGA gain in dB, 0-62 step 2 (overrides config)")
        ("amp-enable", po::value<bool>(), "enable HackRF front-end amp (overrides config)")
        ("serial", po::value<std::string>(), "HackRF serial number (overrides config)")
        ("gain-mode", po::value<std::string>(),
            "PlutoSDR gain_control_mode: manual|fast_attack|slow_attack|hybrid (overrides config)")
        ("rx-gain", po::value<uint32_t>(), "PlutoSDR manual RX gain in dB, manual mode only (overrides config)")
        ("uri", po::value<std::string>(), "PlutoSDR context URI, e.g. usb:5.7.5, ip:192.168.2.1 (overrides config)")
        ("audio-device", po::value<std::string>()->default_value("default"), "ALSA playback device")
        ("stereo", po::bool_switch()->default_value(false),
            "decode WFM stereo (19 kHz pilot + 38 kHz DSB-SC) instead of mono");
    // clang-format on

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error &e) {
        std::cerr << "argument error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    const std::string config_path = vm["config"].as<std::string>();

    config::signal_source_config cfg;
    try {
        cfg = config::load_signal_source_config(config_path);
    } catch (const std::exception &e) {
        std::cerr << "failed to load config '" << config_path << "': " << e.what() << std::endl;
        return 1;
    }

    if (vm.count("frequency")) {
        cfg.frequency_hz = vm["frequency"].as<uint64_t>();
    }
    if (vm.count("lna-gain")) {
        cfg.lna_gain_db = vm["lna-gain"].as<uint32_t>();
    }
    if (vm.count("vga-gain")) {
        cfg.vga_gain_db = vm["vga-gain"].as<uint32_t>();
    }
    if (vm.count("amp-enable")) {
        cfg.amp_enable = vm["amp-enable"].as<bool>();
    }
    if (vm.count("serial")) {
        cfg.serial_number = vm["serial"].as<std::string>();
    }
    if (vm.count("gain-mode")) {
        cfg.gain_control_mode = vm["gain-mode"].as<std::string>();
    }
    if (vm.count("rx-gain")) {
        cfg.rx_gain_db = vm["rx-gain"].as<uint32_t>();
    }
    if (vm.count("uri")) {
        cfg.uri = vm["uri"].as<std::string>();
    }

    print_config(cfg);
    std::cout << "  audio mode    : " << (vm["stereo"].as<bool>() ? "stereo" : "mono") << std::endl;

    std::unique_ptr<hardware::signal_source> source = hardware::create_signal_source(cfg);
    if (!source) {
        std::cerr << "signal source type from config is not supported yet" << std::endl;
        return 1;
    }

    if (!source->open()) {
        std::cerr << "failed to open signal source" << std::endl;
        return 1;
    }

    const bool stereo = vm["stereo"].as<bool>();

    // Rate actually leaving the channel filter/decimator, feeding the rest
    // of the chain (demodulator, stereo decode, de-emphasis, audio resample).
    const uint32_t decimated_rate_hz = capture_rate_hz(cfg) / dsp::filters::kLowpass2mTo200kDecim;

    config::fm_demodulator_config demod_cfg;
    demod_cfg.sample_rate_hz = decimated_rate_hz;

    config::deemphasis_filter_config deemph_cfg;
    deemph_cfg.sample_rate_hz = decimated_rate_hz;

    config::stereo_decoder_config stereo_cfg;
    stereo_cfg.sample_rate_hz = decimated_rate_hz;

    config::rds_decoder_config rds_cfg;
    rds_cfg.sample_rate_hz = decimated_rate_hz;

    config::audio_sink_config sink_cfg;
    sink_cfg.device = vm["audio-device"].as<std::string>();
    sink_cfg.channels = stereo ? 2 : 1;

    config::resampler_config audio_resample_cfg;
    audio_resample_cfg.input_sample_rate_hz = decimated_rate_hz;
    audio_resample_cfg.output_sample_rate_hz = sink_cfg.sample_rate_hz;

    std::unique_ptr<hardware::audio_sink> sink = hardware::create_audio_sink(sink_cfg);
    if (!sink->open()) {
        std::cerr << "failed to open audio sink" << std::endl;
        return 1;
    }

    dsp::iq_converter iq_conv(source->sample_format());
    dsp::equiripple_filter channel_filter;
    dsp::quadrature_demodulator demodulator(demod_cfg);

    // Mono path, or the L channel's de-emphasis/resample when stereo.
    dsp::rc_deemphasis_filter deemphasis_l(deemph_cfg);
    dsp::audio_resampler audio_rs_l(audio_resample_cfg);

    // Only used when --stereo is set: pilot-locked decode plus the R
    // channel's own de-emphasis/resample chain.
    std::unique_ptr<dsp::stereo_decoder> stereo_dec;
    std::unique_ptr<dsp::rc_deemphasis_filter> deemphasis_r;
    std::unique_ptr<dsp::audio_resampler> audio_rs_r;
    if (stereo) {
        stereo_dec = std::make_unique<dsp::stereo_decoder>(stereo_cfg);
        deemphasis_r = std::make_unique<dsp::rc_deemphasis_filter>(deemph_cfg);
        audio_rs_r = std::make_unique<dsp::audio_resampler>(audio_resample_cfg);
    }

    // Diagnostic text output only, so RDS runs always-on without a CLI flag.
    dsp::rds_demodulator rds_demod(rds_cfg);
    dsp::rds_group_decoder rds_decoder;
    std::vector<uint8_t> rds_bits;
    bool rds_pi_printed = false;

    // Reused across callback invocations to avoid per-call heap churn on the
    // capture thread.
    std::vector<std::complex<float>> iq_samples;
    std::vector<std::complex<float>> filtered_samples;
    std::vector<float> demod_samples; // composite baseband
    std::vector<float> stereo_interleaved;
    std::vector<float> channel_l;
    std::vector<float> channel_r;
    std::vector<float> deemph_l_samples;
    std::vector<float> deemph_r_samples;
    std::vector<float> audio_l;
    std::vector<float> audio_r;
    std::vector<float> audio_samples; // final buffer handed to the playback queue

    // Up to ~2 s of buffered audio; absorbs jitter between capture and
    // playback without letting latency creep up unboundedly.
    audio_queue queue(sink_cfg.sample_rate_hz * sink_cfg.channels * 2);

    // Dedicated playback thread: the only place calling sink->write(), so a
    // slow ALSA write can never stall the capture callback. Batches into
    // >=100 ms chunks - tiny per-burst writes leave too little margin
    // against scheduling jitter and cause XRUNs.
    std::thread playback_thread([&queue, &sink, &sink_cfg]() {
        const std::size_t min_write_samples = sink_cfg.sample_rate_hz * sink_cfg.channels / 10; // 100 ms
        std::vector<float> pending;
        std::vector<float> chunk;

        while (queue.pop_all(chunk)) {
            pending.insert(pending.end(), chunk.begin(), chunk.end());
            if (pending.size() >= min_write_samples) {
                sink->write(pending.data(), pending.size());
                pending.clear();
            }
        }

        if (!pending.empty()) {
            sink->write(pending.data(), pending.size());
        }
    });

    const bool started = source->start([&](const uint8_t *data, std::size_t length) {
        iq_conv.process(data, length, iq_samples);
        channel_filter.process(iq_samples.data(), iq_samples.size(), filtered_samples);
        demodulator.process(filtered_samples.data(), filtered_samples.size(), demod_samples);

        rds_demod.process(demod_samples.data(), demod_samples.size(), rds_bits);
        if (!rds_bits.empty()) {
            rds_decoder.push_bits(rds_bits.data(), rds_bits.size());

            if (!rds_pi_printed && rds_decoder.has_pi()) {
                rds_pi_printed = true;
                std::cout << "RDS PI: 0x" << std::hex << std::setw(4) << std::setfill('0') << rds_decoder.pi()
                           << std::dec << std::setfill(' ') << std::endl;
            }
            if (rds_decoder.consume_ps_changed()) {
                std::cout << "RDS PS: '" << rds_decoder.program_service() << "'" << std::endl;
            }
            if (rds_decoder.consume_rt_changed()) {
                std::cout << "RDS RT: '" << rds_decoder.radio_text() << "'" << std::endl;
            }
        }

        if (stereo) {
            stereo_dec->process(demod_samples.data(), demod_samples.size(), stereo_interleaved);

            const std::size_t n_frames = stereo_interleaved.size() / 2;
            channel_l.resize(n_frames);
            channel_r.resize(n_frames);
            for (std::size_t i = 0; i < n_frames; ++i) {
                channel_l[i] = stereo_interleaved[2 * i];
                channel_r[i] = stereo_interleaved[2 * i + 1];
            }

            deemphasis_l.process(channel_l.data(), channel_l.size(), deemph_l_samples);
            deemphasis_r->process(channel_r.data(), channel_r.size(), deemph_r_samples);

            audio_rs_l.process(deemph_l_samples.data(), deemph_l_samples.size(), audio_l);
            audio_rs_r->process(deemph_r_samples.data(), deemph_r_samples.size(), audio_r);

            const std::size_t n_out = std::min(audio_l.size(), audio_r.size());
            audio_samples.resize(n_out * 2);
            for (std::size_t i = 0; i < n_out; ++i) {
                audio_samples[2 * i] = audio_l[i];
                audio_samples[2 * i + 1] = audio_r[i];
            }
        } else {
            deemphasis_l.process(demod_samples.data(), demod_samples.size(), deemph_l_samples);
            audio_rs_l.process(deemph_l_samples.data(), deemph_l_samples.size(), audio_samples);
        }

        if (!audio_samples.empty()) {
            queue.push(audio_samples.data(), audio_samples.size());
        }
    });

    if (!started) {
        std::cerr << "failed to start reception" << std::endl;
        queue.stop();
        playback_thread.join();
        sink->close();
        source->close();
        return 1;
    }

    std::signal(SIGINT, handle_sigint);
    std::cout << "receiving and playing audio, press Ctrl+C to stop" << std::endl;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    source->stop();
    source->close();
    queue.stop();
    playback_thread.join();
    sink->close();

    return 0;
}

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

#include <atomic>
#include <chrono>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "config/audio_sink_config.h"
#include "config/deemphasis_filter_config.h"
#include "config/fm_demodulator_config.h"
#include "config/json_config_loader.h"
#include "config/resampler_config.h"
#include "dsp/audio_resampler.h"
#include "dsp/equiripple_filter.h"
#include "dsp/filters/lowpass_2m_to_200k.hpp"
#include "dsp/iq_converter.h"
#include "dsp/quadrature_demodulator.h"
#include "dsp/rc_deemphasis_filter.h"
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

} // namespace

int main(int argc, char **argv) {
    po::options_description desc("wfm_receiver_demo options");
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
        ("audio-device", po::value<std::string>()->default_value("default"), "ALSA playback device");
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

    std::unique_ptr<hardware::signal_source> source = hardware::create_signal_source(cfg);
    if (!source) {
        std::cerr << "signal source type from config is not supported yet" << std::endl;
        return 1;
    }

    if (!source->open()) {
        std::cerr << "failed to open signal source" << std::endl;
        return 1;
    }

    // Rate actually leaving the channel filter/decimator, feeding the rest
    // of the chain (demodulator, de-emphasis, final audio resample).
    const uint32_t decimated_rate_hz = capture_rate_hz(cfg) / dsp::filters::kLowpass2mTo200kDecim;

    config::fm_demodulator_config demod_cfg;
    demod_cfg.sample_rate_hz = decimated_rate_hz;

    config::deemphasis_filter_config deemph_cfg;
    deemph_cfg.sample_rate_hz = decimated_rate_hz;

    config::audio_sink_config sink_cfg;
    sink_cfg.device = vm["audio-device"].as<std::string>();

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
    dsp::rc_deemphasis_filter deemphasis(deemph_cfg);
    dsp::audio_resampler audio_rs(audio_resample_cfg);

    // Reused across callback invocations to avoid per-call heap churn on the
    // capture thread.
    std::vector<std::complex<float>> iq_samples;
    std::vector<std::complex<float>> filtered_samples;
    std::vector<float> demod_samples;
    std::vector<float> deemph_samples;
    std::vector<float> audio_samples;

    // NB: this callback runs on the source's own capture thread and calls
    // into ALSA's blocking write directly - simplest way to get audio
    // flowing end to end, at the cost of the capture thread stalling if
    // playback ever falls behind. Good enough for now; a producer/consumer
    // queue between capture and playback would decouple them properly.
    const bool started = source->start([&](const uint8_t *data, std::size_t length) {
        iq_conv.process(data, length, iq_samples);
        channel_filter.process(iq_samples.data(), iq_samples.size(), filtered_samples);
        demodulator.process(filtered_samples.data(), filtered_samples.size(), demod_samples);
        deemphasis.process(demod_samples.data(), demod_samples.size(), deemph_samples);
        audio_rs.process(deemph_samples.data(), deemph_samples.size(), audio_samples);

        if (!audio_samples.empty()) {
            sink->write(audio_samples.data(), audio_samples.size());
        }
    });

    if (!started) {
        std::cerr << "failed to start reception" << std::endl;
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
    sink->close();

    return 0;
}

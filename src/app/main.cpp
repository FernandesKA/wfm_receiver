/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Demo: receive IQ data from the configured signal source and print
 *        the average sample amplitude once per second.
 * @version 0.1
 * @date 2026-07-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <boost/program_options.hpp>

#include "config/json_config_loader.h"
#include "dsp/amplitude_monitor.h"
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
                       << "  RX gain       : " << cfg.rx_gain_db << " dB\n"
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
        ("rx-gain", po::value<uint32_t>(), "PlutoSDR manual RX gain in dB (overrides config)")
        ("uri", po::value<std::string>(), "PlutoSDR context URI, e.g. usb:5.7.5, ip:192.168.2.1 (overrides config)");
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

    dsp::amplitude_monitor monitor;
    const hardware::iq_sample_format format = source->sample_format();
    const bool started = source->start([&monitor, format](const uint8_t *data, std::size_t length) {
        monitor.add_samples(data, length, format);
    });

    if (!started) {
        std::cerr << "failed to start reception" << std::endl;
        source->close();
        return 1;
    }

    std::signal(SIGINT, handle_sigint);
    std::cout << "receiving, press Ctrl+C to stop" << std::endl;

    constexpr auto report_period = std::chrono::seconds(1);
    while (g_running.load()) {
        std::this_thread::sleep_for(report_period);
        std::cout << "avg amplitude: " << monitor.get_and_reset_average() << std::endl;
    }

    source->stop();
    source->close();

    return 0;
}

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
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "dsp/amplitude_monitor.h"
#include "hw/signal_source.h"

namespace {

    std::atomic<bool> g_running{true};

    void handle_sigint(int) {
        g_running.store(false);
    }

} // namespace

int main(int argc, char **argv) {
    const std::string config_path = argc > 1 ? argv[1] : "configs/signal_source.json";

    std::unique_ptr<hardware::signal_source> source;
    try {
        source = hardware::create_signal_source_from_file(config_path);
    } catch (const std::exception &e) {
        std::cerr << "failed to load config '" << config_path << "': " << e.what() << std::endl;
        return 1;
    }

    if (!source) {
        std::cerr << "signal source type from config is not supported yet" << std::endl;
        return 1;
    }

    if (!source->open()) {
        std::cerr << "failed to open signal source" << std::endl;
        return 1;
    }

    dsp::amplitude_monitor monitor;
    const bool started = source->start([&monitor](const uint8_t *data, std::size_t length) {
        monitor.add_samples(data, length);
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

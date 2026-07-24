/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <csignal>
#include <getopt.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <ebus.hpp>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

std::atomic<bool> keep_running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    keep_running.store(false);
  }
}

void usage() {
  std::cout << "Usage: ebusproxy [options]\n\n"
            << "eBUS Enhanced test application/driver\n\n"
            << "Options:\n"
            << "  -p, --plain-port <port>     Port for plain eBUS connections (default: 3333)\n"
            << "  -e, --enhanced-port <port>  Port for enhanced ebusd connections (default: 3335)\n"
#if !EBUS_SIMULATION
            << "  -d, --device <dev>          Serial device path (default: /dev/null)\n"
#endif
            << "  -s, --syn-gen <0|1>         Enable (1) or disable (0) SYN generation (default: 1)\n"
            << "  -v, --verbose               Enable verbose logging\n"
            << "  -h, --help                  Show this help page\n"
            << std::endl;
}

int main(int argc, char* argv[]) {
  // Set up signal handling
  ::signal(SIGINT, signal_handler);
  ::signal(SIGTERM, signal_handler);

  uint16_t plain_port = 3333;
  uint16_t enhanced_port = 3335;
  std::string device_path = "/dev/null";
  bool syn_gen = true;
  ebus::LogLevel log_level = ebus::LogLevel::debug;

  static struct option options[] = {
      {"plain-port", required_argument, nullptr, 'p'},
      {"enhanced-port", required_argument, nullptr, 'e'},
      {"device", required_argument, nullptr, 'd'},
      {"syn-gen", required_argument, nullptr, 's'},
      {"verbose", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "p:e:d:s:vh", options, nullptr)) != -1) {
    switch (option) {
      case 'p':
        plain_port = static_cast<uint16_t>(std::stoi(optarg));
        break;
      case 'e':
        enhanced_port = static_cast<uint16_t>(std::stoi(optarg));
        break;
      case 'd':
        device_path = optarg;
        break;
      case 's':
        syn_gen = (std::stoi(optarg) != 0);
        break;
      case 'v':
        log_level = ebus::LogLevel::debug;
        break;
      case 'h':
        usage();
        return EXIT_SUCCESS;
      default:
        usage();
        return EXIT_FAILURE;
    }
  }

  // --- Setup Logging Sink ---
  ebus::Controller::setLogSink([](ebus::LogLevel level, std::string_view msg) {
    std::cout << "[LIB][" << ebus::toString(level) << "] " << msg << std::endl;
  });

  // --- Configuration ---
  ebus::EbusConfig config;
  config.runtime.address = 0x31;  // Standard test address
  config.runtime.diagnostics.level = log_level;
  config.runtime.bus.syn_gen = syn_gen;
  config.runtime.bus.watchdog_timeout_ms = 250;

  // Configure ClientManager server
  config.runtime.network.enable_server = true;
  config.runtime.network.port_regular = plain_port;
  config.runtime.network.port_enhanced = enhanced_port;
  config.runtime.network.port_readonly = 3334; // standard readonly port

#if !EBUS_SIMULATION
  config.bus.device = device_path;
#endif

  std::cout << "Starting eBUS Controller with configuration:\n"
            << "  Plain client port   : " << plain_port << "\n"
            << "  Enhanced client port: " << enhanced_port << "\n"
#if EBUS_SIMULATION
            << "  Bus Mode            : SIMULATION\n"
#else
            << "  Bus Mode            : REAL HARDWARE (" << device_path << ")\n"
#endif
            << "  SYN Generation      : " << (syn_gen ? "ENABLED" : "DISABLED") << "\n"
            << std::endl;

  ebus::Controller controller(config);

  // Set up a protocol callback to see what telegrams are passing on the bus
  controller.setProtocolCallback([](const ebus::ProtocolInfo& info) {
    if (info.is_error) {
      std::cout << "[Bus Error] " << ebus::toString(info.protocol_error) << std::endl;
    } else {
      std::cout << "[Bus Telegram] Type=" << ebus::toString(info.telegram_type)
                << " Master=" << ebus::byteToHex(info.master_view)
                << " Slave=" << ebus::byteToHex(info.slave_view) << std::endl;
    }
  });

  if (!controller.start()) {
    std::cerr << "Failed to start the eBUS controller." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "eBUS Controller is running. Press Ctrl+C to exit.\n" << std::endl;

  while (keep_running.load()) {
    std::this_thread::sleep_for(100ms);
  }

  std::cout << "Shutting down eBUS Controller..." << std::endl;
  controller.stop();
  std::cout << "Shutdown complete." << std::endl;

  return EXIT_SUCCESS;
}

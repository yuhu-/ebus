/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The reader interprets all incoming values ​​as eBUS data. As a
// console-based program, it accepts input from standard input via pipe as well
// as reading from files or a TCP socket. The data is checked for correctness
// and output to standard output in the same canonical line format the
// adapter logs (date + raw telegram, CRC/ACK stripped by default).
// Dumping of binary values ​​is also supported.
// It automatically detects and supports the ebusd Enhanced Protocol.

#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <ebus/data_types.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/sequence.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "app/enhanced_protocol.hpp"
#include "core/telegram.hpp"

using namespace ebus::detail;

constexpr const char* ansi_reset = "\033[0m";
constexpr const char* ansi_bold = "\033[1m";

constexpr uint8_t enhanced_symbol = 0xc6;
constexpr int enhanced_threshold = 2;

bool bold = false;
bool dump = false;
bool full = false;
bool noerror = false;
bool notime = false;
bool json_output = false;
bool pretty = false;

const char* timestamp() {
  static char time[24];
  struct timeval tv;
  struct tm tm;

  if (gettimeofday(&tv, nullptr) != 0) {
    std::cerr << "the current time could not be retrieved" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  if (localtime_r(&tv.tv_sec, &tm) == nullptr) {
    std::cerr << "localtime_r failed" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::snprintf(time, sizeof(time), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);

  return time;
}

void printTelegram(const Telegram& tel) {
  std::string out;
  if (!notime) {
    out += timestamp();
    out += " ";
  }
  if (full) {
    // Complete message in wire order, CRC/ACK bytes included. Data bytes
    // are emphasized when bold is enabled (and stdout is a terminal).
    out += ebus::toString(tel.getSourceAddress());
    out += ebus::toString(tel.getTargetAddress());
    out += ebus::toString(tel.getPrimaryCommand());
    out += ebus::toString(tel.getSecondaryCommand());
    out += ebus::toString(tel.getMasterNumberBytes());
    if (tel.getMasterNumberBytes() > 0) {
      if (bold) out += ansi_bold;
      ebus::toString(out, tel.getMasterDataBytes());
      if (bold) out += ansi_reset;
    }
    out += ebus::toString(tel.getMasterCRC());
    if (tel.getType() != ebus::TelegramType::broadcast) {
      out += ebus::toString(tel.getSlaveACK());
      if (tel.getType() == ebus::TelegramType::master_slave) {
        out += ebus::toString(tel.getSlaveNumberBytes());
        if (tel.getSlaveNumberBytes() > 0) {
          if (bold) out += ansi_bold;
          ebus::toString(out, tel.getSlaveDataBytes());
          if (bold) out += ansi_reset;
        }
        out += ebus::toString(tel.getSlaveCRC());
        out += ebus::toString(tel.getMasterACK());
      }
    }
  } else {
    // Canonical adapter-log format: raw data, CRC/ACK bytes stripped.
    out += ebus::toString(tel.getSourceAddress());
    out += ebus::toString(tel.getTargetAddress());
    out += ebus::toString(tel.getPrimaryCommand());
    out += ebus::toString(tel.getSecondaryCommand());
    out += ebus::toString(tel.getMasterNumberBytes());
    ebus::toString(out, tel.getMasterDataBytes());
    if (tel.getType() == ebus::TelegramType::master_slave) {
      out += " ";
      ebus::toString(out, tel.getSlaveDataBytes());
    }
  }
  std::cout << out << std::endl;
}

void printError(const Telegram& tel, const ebus::Sequence& sequence) {
  std::string out;
  if (!notime) {
    out += timestamp();
    out += " ";
  }
  ebus::toString(out, sequence);
  out += "\nERROR ";
  tel.toString(out);
  std::cout << out << std::endl;
}

void collect(uint8_t byte) {
  static ebus::Sequence sequence;

  if (byte == ebus::Symbols::syn) {
    static bool running = false;
    if (sequence.size() > 0 && running) {
      Telegram tel(sequence);
      if (json_output) {
        // The JsonWriter streams directly to the visitor.
        JsonWriter writer([&](std::string_view s) { std::cout << s; }, pretty);
        tel.toJson(writer);
        std::cout << std::endl;
      } else if (tel.isValid()) {
        printTelegram(tel);
      } else if (!noerror) {
        printError(tel, sequence);
      }
      sequence.clear();
    }
    running = true;
  } else {
    sequence.push_back(byte);
  }
}

int connect(const char* hostname, const char* port, int max_retries = 5,
            int delay_seconds = 10) {
  int attempt = 0;
  while (attempt < max_retries) {
    struct addrinfo hints, *addrs;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    const int status = getaddrinfo(hostname, port, &hints, &addrs);
    if (status != 0) {
      std::cerr << gai_strerror(status) << std::endl;
      return -1;
    }

    for (const struct addrinfo* addr = addrs; addr != nullptr;
         addr = addr->ai_next) {
      int sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (sfd > 0) {
        if (::connect(sfd, addr->ai_addr, addr->ai_addrlen) == 0) {
          freeaddrinfo(addrs);
          return sfd;
        }
        close(sfd);
      }
    }
    freeaddrinfo(addrs);

    ++attempt;
    std::cerr << "Connection failed (attempt " << attempt << " of "
              << max_retries << ").";
    if (attempt < max_retries) {
      std::cerr << " Retrying in " << delay_seconds << " seconds..."
                << std::endl;
      sleep(delay_seconds);
    } else {
      std::cerr << " Giving up." << std::endl;
    }
  }
  return -1;
}

void run(const char* hostname, const char* port, int max_retries = 5) {
  while (true) {
    int sfd = connect(hostname, port, max_retries);
    if (sfd < 0) {
      std::cerr << "Could not connect to " << hostname << ":" << port
                << std::endl;
      std::exit(EXIT_FAILURE);
    }
    std::cerr << "Connected to " << hostname << ":" << port << std::endl;

    uint8_t data[2]{};
    bool connection_ok = true;
    bool mode_enhanced = false;
    int enhanced_seq_count = 0;
    bool waiting_for_c6 = true;  // true: expect 0xC6, false: expect 0xAA

    while (connection_ok) {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(sfd, &readfds);

      struct timeval tv;
      tv.tv_sec = 10;  // 10 second timeout (adjust as needed)
      tv.tv_usec = 0;

      int ret = select(sfd + 1, &readfds, nullptr, nullptr, &tv);
      if (ret < 0) {
        std::cerr << "select() error: " << std::strerror(errno) << std::endl;
        connection_ok = false;
      } else if (ret == 0) {
        std::cerr << "Timeout: no data received for 10 seconds." << std::endl;
        connection_ok = false;
      } else if (FD_ISSET(sfd, &readfds)) {
        if (!mode_enhanced) {
          ssize_t datalen = recv(sfd, data, 1, 0);
          if (datalen == -1) {
            std::cerr << "An error occurred while receiving: "
                      << std::strerror(errno) << std::endl;
            connection_ok = false;
          } else if (datalen == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            connection_ok = false;
          } else {
            uint8_t byte = static_cast<uint8_t>(data[0]);
            if (waiting_for_c6) {
              if (byte == enhanced_symbol) {
                waiting_for_c6 = false;  // now expect 0xaa
              } else {
                enhanced_seq_count = 0;
                waiting_for_c6 = true;
              }
            } else {  // waiting for 0xAA
              if (byte == ebus::Symbols::syn) {
                enhanced_seq_count++;
                if (enhanced_seq_count >= enhanced_threshold) {
                  mode_enhanced = true;
                  std::cerr << "*** Switching to ENHANCED mode! ***"
                            << std::endl;
                }
                waiting_for_c6 = true;  // next, expect 0xC6 again
              } else {
                enhanced_seq_count = 0;
                waiting_for_c6 = true;
              }
            }
            if (dump) {
              std::cout << byte;
            } else {
              collect(byte);
              // collect now prints directly
            }
            std::fflush(stdout);
          }
        } else {
          uint8_t enhanced_byte;
          ssize_t datalen = recv(sfd, data, 2, MSG_PEEK);
          if (datalen < 2) {
            // Not enough data, treat as disconnect or wait for more
            connection_ok = true;
          } else {
            if (enhanced::Protocol::isValidSequence(data[0], data[1])) {
              // Valid enhanced protocol
              recv(sfd, data, 2, 0);  // consume bytes
              uint8_t cmd;
              uint8_t val;
              enhanced::Protocol::decode(data, cmd, val);
              enhanced_byte = val;
              if (dump) {
                std::cout << enhanced_byte;
              } else {
                collect(enhanced_byte);
              }
              std::fflush(stdout);
            } else if (data[0] < 0x80) {
              // Short form: just a data byte, no prefix
              recv(sfd, data, 1, 0);  // consume one byte
              enhanced_byte = data[0];
              if (dump) {
                std::cout << enhanced_byte;
              } else {
                collect(enhanced_byte);
                // collect now prints directly
              }
              std::fflush(stdout);
            } else {
              // Invalid signature, skip one byte
              recv(sfd, data, 1, 0);
            }
          }
        }
      }
    }
    close(sfd);
    std::cerr << "Disconnected. Attempting to reconnect..." << std::endl;
    sleep(2);  // Wait before reconnecting
  }
}

void usage() {
  std::cout << "Usage: ebusread [options] <stdin|device|file|host:port>";
  std::cout << std::endl;
  std::cout << "eBUS binary data reader" << std::endl;
  std::cout << "Supports automatic detection of the Enhanced Protocol when "
               "connecting to ebusd"
            << std::endl;
  std::cout << "Default output is one canonical line per valid telegram, "
               "matching the adapter log:"
            << std::endl;
  std::cout << "  <date> <master without CRC> [<slave data without CRC>]"
            << std::endl;
  std::cout << "  -f, --full       complete message in wire order (CRC/ACK "
               "included, data bytes bold with -b)"
            << std::endl;
  std::cout << "  -b, --bold       bold data bytes (full mode, terminal only)"
            << std::endl;
  std::cout << "  -d, --dump       dump binary values to stdout" << std::endl;
  std::cout << "  -e, --noerror    suppress errors" << std::endl;
  std::cout << "  -n, --notime     suppress timestamp" << std::endl;
  std::cout << "  -j, --json       output telegrams as JSON" << std::endl;
  std::cout << "  -p, --pretty     pretty print JSON output" << std::endl;
  std::cout << "  -h, --help       show this page" << std::endl;
}

int main(int argc, char* argv[]) {
  static struct option options[] = {{"bold", no_argument, nullptr, 'b'},
                                    {"dump", no_argument, nullptr, 'd'},
                                    {"full", no_argument, nullptr, 'f'},
                                    {"noerror", no_argument, nullptr, 'e'},
                                    {"notime", no_argument, nullptr, 'n'},
                                    {"json", no_argument, nullptr, 'j'},
                                    {"pretty", no_argument, nullptr, 'p'},
                                    {"help", no_argument, nullptr, 'h'},
                                    {nullptr, 0, nullptr, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "bdfenjph", options, nullptr)) !=
         -1) {
    switch (option) {
      case 'b':
        bold = true;
        break;
      case 'd':
        dump = true;
        break;
      case 'f':
        full = true;
        break;
      case 'e':
        noerror = true;
        break;
      case 'n':
        notime = true;
        break;
      case 'j':
        json_output = true;
        break;
      case 'p':
        pretty = true;
        break;
      case 'h':
      case '?':
        usage();
        std::exit(EXIT_SUCCESS);
      default:
        std::cerr << "the specified option is unknown" << std::endl;
        std::exit(EXIT_FAILURE);
        break;
    }
  }

  // Bold escapes only make sense on a terminal; piped output stays clean
  // for diff/grep against the adapter log.
  if (bold && isatty(STDOUT_FILENO) == 0) bold = false;

  if (argv[optind] != nullptr) {
    std::string tmp = argv[optind];
    size_t pos = tmp.find(':');
    if (pos == std::string::npos) {
      std::ifstream stream(argv[optind], std::ios::binary);
      if (stream.is_open() == true) {
        while (stream.peek() != EOF) {
          unsigned char byte = stream.get();
          collect(byte);
          // collect now prints directly
        }
        stream.close();
      } else {
        std::cerr << "file '" << argv[optind] << "' not found" << std::endl;
        std::exit(EXIT_FAILURE);
      }
    } else {
      std::string hostname = tmp.substr(0, pos);
      std::string port = tmp.substr(pos + 1);

      if (hostname.empty() || port.empty()) {
        std::cerr << "hostname or port cannot be empty" << std::endl;
        std::exit(EXIT_FAILURE);
      }

      run(hostname.c_str(), port.c_str(), 5);  // 5 retries per disconnect
    }
  } else if (argv[optind] == nullptr && isatty(STDIN_FILENO)) {
    usage();
    std::exit(EXIT_SUCCESS);
  } else {
    while (std::cin.good() && !std::cin.eof()) {
      int byte = std::cin.get();
      if (byte == EOF) break;
      collect(static_cast<uint8_t>(byte));
    }
  }
  return EXIT_SUCCESS;
}

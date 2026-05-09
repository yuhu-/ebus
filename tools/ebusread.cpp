/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The reader interprets all incoming values ​​as eBUS data. As a
// console-based program, it accepts input from standard input via pipe as well
// as reading from files or a TCP socket. The data is checked for correctness
// and output to standard output. Various formatting options are available for
// attractive output. Dumping of binary values ​​is also supported.

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
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "core/telegram.hpp"

constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";

constexpr const char* RED = "\033[31m";
constexpr const char* GREEN = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* BLUE = "\033[34m";
constexpr const char* MAGENTA = "\033[35m";
constexpr const char* CYAN = "\033[36m";

constexpr uint8_t ENHANCED_SYM = 0xc6;
constexpr int ENHANCED_THRESHOLD = 2;

bool bold = false;
bool color = false;
bool dump = false;
bool noerror = false;
bool notime = false;
bool parse = false;
bool raw = false;
bool split = false;
bool type = false;

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

std::string services(ebus::ByteView master, ebus::ByteView slave) {
  std::ostringstream ostr;
  if (master[2] == 0x07 && master[3] == 0x00) {
    ostr << "0700: 20";
    ostr << ebus::toString(master[13]);
    ostr << "-";
    ostr << ebus::toString(master[11]);
    ostr << "-";
    ostr << ebus::toString(master[10]);
    ostr << " ";
    ostr << ebus::toString(master[9]);
    ostr << ":";
    ostr << ebus::toString(master[8]);
    ostr << ":";
    ostr << ebus::toString(master[7]);
    ostr << " - ";
    ostr << ebus::toString(
        *ebus::decode(ebus::DataType::data2b, ebus::range(master, 5, 2)),
        " °C");
    ostr << " °C";
  } else if (master[2] == 0x07 && master[3] == 0x04) {
    ostr << "0704: " + ebus::toString(master[1]);
    ostr << " MF=";
    ostr << ebus::toString(ebus::range(slave, 1, 1));
    ostr << " ID=";
    ostr << ebus::byteToChar(ebus::range(slave, 2, 5));
    ostr << " SW=";
    ostr << ebus::toString(ebus::range(slave, 7, 2));
    ostr << " HW=";
    ostr << ebus::toString(ebus::range(slave, 9, 2));
  } else if (master[2] == 0xb5 && master[3] == 0x16 && master[4] == 0x08) {
    ostr << "b51608: 20";
    ostr << ebus::toString(master[12]);
    ostr << "-";
    ostr << ebus::toString(master[10]);
    ostr << "-";
    ostr << ebus::toString(master[9]);
    ostr << " ";
    ostr << ebus::toString(master[8]);
    ostr << ":";
    ostr << ebus::toString(master[7]);
    ostr << ":";
    ostr << ebus::toString(master[6]);
  } else if (master[2] == 0xb5 && master[3] == 0x16 && master[4] == 0x03 &&
             master[5] == 0x01) {
    ostr << "b5160301: ";
    ostr << ebus::toString(
        *ebus::decode(ebus::DataType::data2b, ebus::range(master, 6, 2)),
        " °C");
  }
  return ostr.str();
}

std::string collect(uint8_t byte) {
  static ebus::Sequence sequence;
  std::string result = "";

  if (raw) std::cout << ebus::toString(byte) << std::endl;

  if (byte == ebus::Symbols::syn) {
    static bool running = false;
    if (sequence.size() > 0 && running) {
      ebus::detail::Telegram tel(sequence);
      std::ostringstream ostr;

      if (tel.isValid()) {
        if (!notime) {
          ostr << timestamp();
          ostr << " ";
        }
        if (type) {
          if (color) ostr << CYAN;
          if (tel.getType() == ebus::TelegramType::master_slave)
            ostr << "MS";
          else if (tel.getType() == ebus::TelegramType::master_master)
            ostr << "MM";
          else
            ostr << "BC";
          if (color) ostr << RESET;
          ostr << " ";
        }
        if (color) ostr << GREEN;
        ostr << ebus::toString(tel.getSourceAddress());
        ostr << ebus::toString(tel.getTargetAddress());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << BLUE;
        ostr << ebus::toString(tel.getPrimaryCommand());
        ostr << ebus::toString(tel.getSecondaryCommand());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << YELLOW;
        ostr << ebus::toString(tel.getMasterNumberBytes());
        if (color) ostr << RESET;
        if (tel.getMasterNumberBytes() > 0) {
          if (split) ostr << " ";

          if (bold) ostr << BOLD;
          ostr << ebus::toString(tel.getMasterDataBytes());
          if (bold) ostr << RESET;
        }
        if (split) ostr << " ";
        if (color) ostr << MAGENTA;
        ostr << ebus::toString(tel.getMasterCRC());
        if (color) ostr << RESET;
        if (tel.getType() != ebus::TelegramType::broadcast) {
          if (split) ostr << " ";
          ostr << ebus::toString(tel.getSlaveACK());
          if (tel.getType() == ebus::TelegramType::master_slave) {
            if (split) ostr << " ";
            if (color) ostr << YELLOW;
            ostr << ebus::toString(tel.getSlaveNumberBytes());
            if (color) ostr << RESET;
            if (tel.getSlaveNumberBytes() > 0) {
              if (split) ostr << " ";
              if (bold) ostr << BOLD;
              ostr << ebus::toString(tel.getSlaveDataBytes());
              if (bold) ostr << RESET;
            }
            if (split) ostr << " ";
            if (color) ostr << MAGENTA;
            ostr << ebus::toString(tel.getSlaveCRC());
            if (color) ostr << RESET;
            if (split) ostr << " ";
            ostr << ebus::toString(tel.getMasterACK());
          }
        }
        if (parse) {
          std::string tmp =
              services(tel.getMaster().toVector(), tel.getSlave().toVector());
          if (!tmp.empty()) {
            ostr << std::endl;
            if (color) ostr << CYAN;
            if (!notime) {
              ostr << "---SERVICE-DETECTED---> ";
              if (type) ostr << "   ";
            }
            ostr << tmp;
            if (color) ostr << RESET;
          }
        }
      } else if (!noerror) {
        if (!notime) {
          ostr << timestamp();
          ostr << " ";
        }
        if (type) ostr << "   ";
        ostr << sequence.toString() << std::endl;
        if (color) ostr << RED;
        if (!notime) {
          ostr << "----ERROR-DETECTED----> ";
          if (type) ostr << "   ";
        }
        ostr << tel.toString();
        if (color) ostr << RESET;
      }

      result = ostr.str();
      if (!notime) {
        ostr << timestamp();
        ostr << " ";
      }
      sequence.clear();
    }
    running = true;
  } else {
    sequence.pushBack(byte);
  }

  return result;
}

int connect(const char* hostname, const char* port, int max_retries = 5,
            int delay_seconds = 10) {
  int attempt = 0;
  while (attempt < max_retries) {
    int sfd = -1, err = 0;
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
      sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (sfd > 0) {
        if (::connect(sfd, addr->ai_addr, addr->ai_addrlen) == 0) {
          freeaddrinfo(addrs);
          return sfd;
        }
        close(sfd);
      } else {
        err = errno;
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

    char data[2];
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
              if (byte == ENHANCED_SYM) {
                waiting_for_c6 = false;  // now expect 0xaa
              } else {
                enhanced_seq_count = 0;
                waiting_for_c6 = true;
              }
            } else {  // waiting for 0xAA
              if (byte == ebus::Symbols::syn) {
                enhanced_seq_count++;
                if (enhanced_seq_count >= ENHANCED_THRESHOLD) {
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
              std::string result = collect(byte);
              if (result.size() > 0) std::cout << result << std::endl;
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
            int b1 = static_cast<uint8_t>(data[0]);
            int b2 = static_cast<uint8_t>(data[1]);
            if ((b1 & 0xc0) == 0xc0 && (b2 & 0xc0) == 0x80) {
              // Valid enhanced protocol
              recv(sfd, data, 2, 0);  // consume bytes
              uint8_t cmd = (b1 >> 2) & 0x0f;
              uint8_t val = ((b1 & 0x03) << 6) | (b2 & 0x3f);
              enhanced_byte = val;
              if (dump) {
                std::cout << enhanced_byte;
              } else {
                std::string result = collect(enhanced_byte);
                if (result.size() > 0) std::cout << result << std::endl;
              }
              std::fflush(stdout);
            } else if (b1 < 0x80) {
              // Short form: just a data byte, no prefix
              recv(sfd, data, 1, 0);  // consume one byte
              enhanced_byte = static_cast<uint8_t>(data[0]);
              if (dump) {
                std::cout << enhanced_byte;
              } else {
                std::string result = collect(enhanced_byte);
                if (result.size() > 0) std::cout << result << std::endl;
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
  std::cout << "  -b, --bold       bold data bytes" << std::endl;
  std::cout << "  -c, --color      colorized output" << std::endl;
  std::cout << "  -d, --dump       dump binary values to stdout" << std::endl;
  std::cout << "  -e, --noerror    suppress errors" << std::endl;
  std::cout << "  -n, --notime     suppress timestamp" << std::endl;
  std::cout << "  -p, --parse      parse known services" << std::endl;
  std::cout << "  -r, --raw        print raw values" << std::endl;
  std::cout << "  -s, --split      split telegram parts" << std::endl;
  std::cout << "  -t, --type       print telegram type" << std::endl;
  std::cout << "  -h, --help       show this page" << std::endl;
}

int main(int argc, char* argv[]) {
  static struct option options[] = {{"bold", no_argument, nullptr, 'b'},
                                    {"color", no_argument, nullptr, 'c'},
                                    {"dump", no_argument, nullptr, 'd'},
                                    {"noerror", no_argument, nullptr, 'e'},
                                    {"notime", no_argument, nullptr, 'n'},
                                    {"parse", no_argument, nullptr, 'p'},
                                    {"raw", no_argument, nullptr, 'r'},
                                    {"split", no_argument, nullptr, 's'},
                                    {"type", no_argument, nullptr, 't'},
                                    {"help", no_argument, nullptr, 'h'},
                                    {nullptr, 0, nullptr, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "bcdefnprsth", options, nullptr)) !=
         -1) {
    switch (option) {
      case 'b':
        bold = true;
        break;
      case 'c':
        color = true;
        break;
      case 'd':
        dump = true;
        break;
      case 'e':
        noerror = true;
        break;
      case 'n':
        notime = true;
        break;
      case 'p':
        parse = true;
        break;
      case 'r':
        raw = true;
        break;
      case 's':
        split = true;
        break;
      case 't':
        type = true;
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

  if (argv[optind] != nullptr) {
    std::string tmp = argv[optind];
    size_t pos = tmp.find(':');
    if (pos == std::string::npos) {
      std::ifstream stream(argv[optind], std::ios::binary);
      if (stream.is_open() == true) {
        while (stream.peek() != EOF) {
          unsigned char byte = stream.get();
          std::string result = collect(byte);
          if (result.size() > 0) std::cout << result << std::endl;
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
      if (std::cin.eof()) break;
      std::string result = collect(static_cast<uint8_t>(byte));
      if (result.size() > 0) std::cout << result << std::endl;
    }
  }

  return EXIT_SUCCESS;
}

/*
 * Copyright (C) 2012-2025 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

// The reader interprets all incoming values ​​as eBUS data. As a
// console-based program, it accepts input from standard input via pipe as well
// as reading from files or a TCP socket. The data is checked for correctness
// and output to standard output. Various formatting options are available for
// attractive output. Dumping of binary values ​​is also supported.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Datatypes.hpp"
#include "Telegram.hpp"

constexpr const char *RESET = "\033[0m";
constexpr const char *BOLD = "\033[1m";

constexpr const char *RED = "\033[31m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *BLUE = "\033[34m";
constexpr const char *MAGENTA = "\033[35m";
constexpr const char *CYAN = "\033[36m";

constexpr uint8_t ENHANCED_SYM = 0xC6;
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

const char *timestamp() {
  static char time[24];
  struct timeval tv;
  struct tm tm;

  if (gettimeofday(&tv, nullptr) != 0) {
    std::cerr << "the current time could not be retrieved" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (localtime_r(&tv.tv_sec, &tm) == nullptr) {
    std::cerr << "localtime_r failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  snprintf(time, sizeof(time), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
           tm.tm_sec, tv.tv_usec / 1000);

  return time;
}

std::string services(const std::vector<uint8_t> &master,
                     const std::vector<uint8_t> &slave) {
  std::ostringstream ostr;
  if (master[2] == 0x07 && master[3] == 0x00) {
    ostr << "0700: 20";
    ostr << ebus::to_string(master[13]);
    ostr << "-";
    ostr << ebus::to_string(master[11]);
    ostr << "-";
    ostr << ebus::to_string(master[10]);
    ostr << " ";
    ostr << ebus::to_string(master[9]);
    ostr << ":";
    ostr << ebus::to_string(master[8]);
    ostr << ":";
    ostr << ebus::to_string(master[7]);
    ostr << " - ";
    ostr << ebus::byte_2_data2b(ebus::range(master, 5, 2));
    ostr << " °C";
  } else if (master[2] == 0x07 && master[3] == 0x04) {
    ostr << "0704: " + ebus::to_string(master[1]);
    ostr << " MF=";
    ostr << ebus::to_string(ebus::range(slave, 1, 1));
    ostr << " ID=";
    ostr << ebus::byte_2_char(ebus::range(slave, 2, 5));
    ostr << " SW=";
    ostr << ebus::to_string(ebus::range(slave, 7, 2));
    ostr << " HW=";
    ostr << ebus::to_string(ebus::range(slave, 9, 2));
  } else if (master[2] == 0xb5 && master[3] == 0x16 && master[4] == 0x08) {
    ostr << "b51608: 20";
    ostr << ebus::to_string(master[12]);
    ostr << "-";
    ostr << ebus::to_string(master[10]);
    ostr << "-";
    ostr << ebus::to_string(master[9]);
    ostr << " ";
    ostr << ebus::to_string(master[8]);
    ostr << ":";
    ostr << ebus::to_string(master[7]);
    ostr << ":";
    ostr << ebus::to_string(master[6]);
  } else if (master[2] == 0xb5 && master[3] == 0x16 && master[4] == 0x03 &&
             master[5] == 0x01) {
    ostr << "b5160301: ";
    ostr << ebus::byte_2_data2b(ebus::range(master, 6, 2));
    ostr << " °C";
  }
  return ostr.str();
}

std::string collect(const uint8_t &byte) {
  static ebus::Sequence sequence;
  std::string result = "";

  if (raw) std::cout << ebus::to_string(byte) << std::endl;

  if (byte == ebus::sym_syn) {
    static bool running = false;
    if (sequence.size() > 0 && running) {
      ebus::Telegram tel(sequence);
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
        ostr << ebus::to_string(tel.getSourceAddress());
        ostr << ebus::to_string(tel.getTargetAddress());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << BLUE;
        ostr << ebus::to_string(tel.getPrimaryCommand());
        ostr << ebus::to_string(tel.getSecondaryCommand());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << YELLOW;
        ostr << ebus::to_string(tel.getMasterNumberBytes());
        if (color) ostr << RESET;
        if (tel.getMasterNumberBytes() > 0) {
          if (split) ostr << " ";

          if (bold) ostr << BOLD;
          ostr << ebus::to_string(tel.getMasterDataBytes());
          if (bold) ostr << RESET;
        }
        if (split) ostr << " ";
        if (color) ostr << MAGENTA;
        ostr << ebus::to_string(tel.getMasterCRC());
        if (color) ostr << RESET;
        if (tel.getType() != ebus::TelegramType::broadcast) {
          if (split) ostr << " ";
          ostr << ebus::to_string(tel.getSlaveACK());
          if (tel.getType() == ebus::TelegramType::master_slave) {
            if (split) ostr << " ";
            if (color) ostr << YELLOW;
            ostr << ebus::to_string(tel.getSlaveNumberBytes());
            if (color) ostr << RESET;
            if (tel.getSlaveNumberBytes() > 0) {
              if (split) ostr << " ";
              if (bold) ostr << BOLD;
              ostr << ebus::to_string(tel.getSlaveDataBytes());
              if (bold) ostr << RESET;
            }
            if (split) ostr << " ";
            if (color) ostr << MAGENTA;
            ostr << ebus::to_string(tel.getSlaveCRC());
            if (color) ostr << RESET;
            if (split) ostr << " ";
            ostr << ebus::to_string(tel.getMasterACK());
          }
        }
        if (parse) {
          std::string tmp =
              services(tel.getMaster().to_vector(), tel.getSlave().to_vector());
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
        ostr << sequence.to_string() << std::endl;
        if (color) ostr << RED;
        if (!notime) {
          ostr << "----ERROR-DETECTED----> ";
          if (type) ostr << "   ";
        }
        ostr << tel.to_string();
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
    sequence.push_back(byte);
  }

  return result;
}

int connect(const char *hostname, const char *port, int max_retries = 5,
            int delay_seconds = 10) {
  int attempt = 0;
  while (attempt < max_retries) {
    int sfd = -1, err = 0;
    struct addrinfo hints, *addrs;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    const int status = getaddrinfo(hostname, port, &hints, &addrs);
    if (status != 0) {
      std::cerr << gai_strerror(status) << std::endl;
      return -1;
    }

    for (const struct addrinfo *addr = addrs; addr != nullptr;
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

void run(const char *hostname, const char *port, int max_retries = 5) {
  while (true) {
    int sfd = connect(hostname, port, max_retries);
    if (sfd < 0) {
      std::cerr << "Could not connect to " << hostname << ":" << port
                << std::endl;
      exit(EXIT_FAILURE);
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
        std::cerr << "select() error: " << strerror(errno) << std::endl;
        connection_ok = false;
      } else if (ret == 0) {
        std::cerr << "Timeout: no data received for 10 seconds." << std::endl;
        connection_ok = false;
      } else if (FD_ISSET(sfd, &readfds)) {
        if (!mode_enhanced) {
          ssize_t datalen = recv(sfd, data, 1, 0);
          if (datalen == -1) {
            std::cerr << "An error occurred while receiving: "
                      << strerror(errno) << std::endl;
            connection_ok = false;
          } else if (datalen == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            connection_ok = false;
          } else {
            uint8_t byte = static_cast<uint8_t>(data[0]);
            if (waiting_for_c6) {
              if (byte == ENHANCED_SYM) {
                waiting_for_c6 = false;  // now expect 0xAA
              } else {
                enhanced_seq_count = 0;
                waiting_for_c6 = true;
              }
            } else {  // waiting for 0xAA
              if (byte == ebus::sym_syn) {
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
            fflush(stdout);
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
              fflush(stdout);
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
              fflush(stdout);
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

int main(int argc, char *argv[]) {
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
        exit(EXIT_SUCCESS);
      default:
        std::cerr << "the specified option is unknown" << std::endl;
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
      }
    } else {
      std::string hostname = tmp.substr(0, pos);
      std::string port = tmp.substr(pos + 1);

      if (hostname.empty() || port.empty()) {
        std::cerr << "hostname or port cannot be empty" << std::endl;
        exit(EXIT_FAILURE);
      }

      run(hostname.c_str(), port.c_str(), 5);  // 5 retries per disconnect
    }
  } else if (argv[optind] == nullptr && isatty(STDIN_FILENO)) {
    usage();
    exit(EXIT_SUCCESS);
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

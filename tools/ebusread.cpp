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
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
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

#include "Datatypes.h"
#include "Telegram.h"

#define RESET "\033[0m"
#define BOLD "\033[1m"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

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
    ostr << ebus::Sequence::to_string(master[13]);
    ostr << "-";
    ostr << ebus::Sequence::to_string(master[11]);
    ostr << "-";
    ostr << ebus::Sequence::to_string(master[10]);
    ostr << " ";
    ostr << ebus::Sequence::to_string(master[9]);
    ostr << ":";
    ostr << ebus::Sequence::to_string(master[8]);
    ostr << ":";
    ostr << ebus::Sequence::to_string(master[7]);
    ostr << " - ";
    ostr << ebus::byte_2_data2b(ebus::Sequence::range(master, 5, 2));
    ostr << " °C";
  } else if (master[2] == 0x07 && master[3] == 0x04) {
    ostr << "0704: " + ebus::Sequence::to_string(master[1]);
    ostr << " MF=";
    ostr << ebus::Sequence::to_string(ebus::Sequence::range(slave, 1, 1));
    ostr << " ID=";
    ostr << ebus::byte_2_string(ebus::Sequence::range(slave, 2, 5));
    ostr << " SW=";
    ostr << ebus::Sequence::to_string(ebus::Sequence::range(slave, 7, 2));
    ostr << " HW=";
    ostr << ebus::Sequence::to_string(ebus::Sequence::range(slave, 9, 2));
  }
  return ostr.str();
}

std::string collect(const uint8_t &byte) {
  static ebus::Sequence sequence;
  std::string result = "";

  if (raw) std::cout << ebus::Sequence::to_string(byte) << std::endl;

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
          if (tel.getType() == ebus::Type::masterSlave)
            ostr << "MS";
          else if (tel.getType() == ebus::Type::masterMaster)
            ostr << "MM";
          else
            ostr << "BC";
          if (color) ostr << RESET;
          ostr << " ";
        }
        if (color) ostr << GREEN;
        ostr << ebus::Sequence::to_string(tel.getSourceAddress());
        ostr << ebus::Sequence::to_string(tel.getTargetAddress());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << BLUE;
        ostr << ebus::Sequence::to_string(tel.getPrimaryCommand());
        ostr << ebus::Sequence::to_string(tel.getSecondaryCommand());
        if (color) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << YELLOW;
        ostr << ebus::Sequence::to_string(tel.getMasterNumberBytes());
        if (color) ostr << RESET;
        if (tel.getMasterNumberBytes() > 0) {
          if (split) ostr << " ";

          if (bold) ostr << BOLD;
          ostr << ebus::Sequence::to_string(tel.getMasterDataBytes());
          if (bold) ostr << RESET;
        }
        if (split) ostr << " ";
        if (color) ostr << MAGENTA;
        ostr << ebus::Sequence::to_string(tel.getMasterCRC());
        if (color) ostr << RESET;
        if (tel.getType() != ebus::Type::broadcast) {
          if (split) ostr << " ";
          ostr << ebus::Sequence::to_string(tel.getSlaveACK());
          if (tel.getType() == ebus::Type::masterSlave) {
            if (split) ostr << " ";
            if (color) ostr << YELLOW;
            ostr << ebus::Sequence::to_string(tel.getSlaveNumberBytes());
            if (color) ostr << RESET;
            if (tel.getSlaveNumberBytes() > 0) {
              if (split) ostr << " ";
              if (bold) ostr << BOLD;
              ostr << ebus::Sequence::to_string(tel.getSlaveDataBytes());
              if (bold) ostr << RESET;
            }
            if (split) ostr << " ";
            if (color) ostr << MAGENTA;
            ostr << ebus::Sequence::to_string(tel.getSlaveCRC());
            if (color) ostr << RESET;
            if (split) ostr << " ";
            ostr << ebus::Sequence::to_string(tel.getMasterACK());
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

void run(const int sfd) {
  char data[1];

  while (true) {
    ssize_t datalen = recv(sfd, data, sizeof(data), 0);

    if (datalen == -1) {
      std::cerr << "an error occurred while receiving" << std::endl;
      exit(EXIT_FAILURE);
    } else if (datalen == 0) {
      std::cerr << "connection closed by peer" << std::endl;
      break;
    }

    for (int i = 0; i < datalen; i++) {
      if (dump) {
        std::cout << data[i];
      } else {
        std::string result = collect(data[i]);
        if (result.size() > 0) std::cout << result << std::endl;
      }
    }

    fflush(stdout);
    memset(&data[0], 0, sizeof(data));
  }
}

int connect(const char *hostname, const char *port) {
  struct addrinfo hints, *addrs;
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  const int status = getaddrinfo(hostname, port, &hints, &addrs);
  if (status != 0) {
    std::cerr << gai_strerror(status) << std::endl;
    exit(EXIT_FAILURE);
  }

  int sfd = 0, err = 0;
  for (const struct addrinfo *addr = addrs; addr != nullptr;
       addr = addr->ai_next) {
    sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sfd > 0) {
      if (connect(sfd, addr->ai_addr, addr->ai_addrlen) == 0) break;
    } else {
      err = errno;
    }
  }

  freeaddrinfo(addrs);

  if (sfd < 0) {
    std::cerr << strerror(err) << std::endl;
    exit(EXIT_FAILURE);
  }

  return sfd;
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

      int sfd = connect(hostname.c_str(), port.c_str());
      run(sfd);
      close(sfd);
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

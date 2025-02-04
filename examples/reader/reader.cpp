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
bool noerror = false;
bool notime = false;
bool raw = false;
bool split = false;
bool type = false;

const char *timestamp() {
  static char time[24];
  struct timeval tv;
  struct tm tm;

  gettimeofday(&tv, nullptr);
  localtime_r(&tv.tv_sec, &tm);

  snprintf(time, sizeof(time), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
           tm.tm_sec, tv.tv_usec / 1000);

  return time;
}

std::string collect(const uint8_t byte) {
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
          if (tel.getType() == ebus::Type::MS)
            ostr << "MS";
          else if (tel.getType() == ebus::Type::MM)
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
        if (split) ostr << " ";
        if (bold) ostr << BOLD;
        ostr << ebus::Sequence::to_string(tel.getMasterDataBytes());
        if (bold) ostr << RESET;
        if (split) ostr << " ";
        if (color) ostr << MAGENTA;
        ostr << ebus::Sequence::to_string(tel.getMasterCRC());
        if (color) ostr << RESET;
        if (tel.getType() != ebus::Type::BC) {
          if (split) ostr << " ";
          ostr << ebus::Sequence::to_string(tel.getSlaveACK());
          if (tel.getType() == ebus::Type::MS) {
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
  char data[1024];

  while (1) {
    ssize_t datalen = recv(sfd, data, sizeof(data), 0);

    for (int i = 0; i < datalen; i++) {
      std::string result = collect(data[i]);

      if (result.size() > 0) std::cout << result << std::endl;
    }

    fflush(stdout);
    memset(&data[0], 0, sizeof(data));
  }
}

int connect(const char *hostname, const char *port) {
  struct addrinfo hints = {0}, *addrs;
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
    sfd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
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
  std::cout << "Usage: reader [options] host:port" << std::endl;
  std::cout << "options:" << std::endl;
  std::cout << "  --bold,    -b   bold data bytes" << std::endl;
  std::cout << "  --color,   -c   colorized output" << std::endl;
  std::cout << "  --noerror  -e   suppress errors" << std::endl;
  std::cout << "  --notime,  -n   suppress timestamp" << std::endl;
  std::cout << "  --raw,     -r   print raw data" << std::endl;
  std::cout << "  --split,   -s   split telegram parts" << std::endl;
  std::cout << "  --type,    -t   print telegram type" << std::endl;
  std::cout << "  --help,    -h   show this page" << std::endl;
}

int main(int argc, char *argv[]) {
  static struct option options[] = {{"bold", no_argument, nullptr, 'b'},
                                    {"color", no_argument, nullptr, 'c'},
                                    {"noerror", no_argument, nullptr, 'e'},
                                    {"notime", no_argument, nullptr, 'n'},
                                    {"raw", no_argument, nullptr, 'r'},
                                    {"split", no_argument, nullptr, 's'},
                                    {"type", no_argument, nullptr, 't'},
                                    {"help", no_argument, nullptr, 'h'},
                                    {nullptr, 0, nullptr, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "bcenrsth", options, nullptr)) !=
         -1) {
    switch (option) {
      case 'b':
        bold = true;
        break;
      case 'c':
        color = true;
        break;
      case 'e':
        noerror = true;
        break;
      case 'n':
        notime = true;
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
        std::cerr << "unknown option" << std::endl;
        exit(EXIT_FAILURE);
        break;
    }
  }

  std::string tmp = argv[argc - 1];
  size_t pos = tmp.find(':');
  if (pos == std::string::npos) {
    usage();
    exit(EXIT_FAILURE);
  }

  std::string hostname = tmp.substr(0, pos);
  std::string port = tmp.substr(pos + 1);

  int sfd = connect(hostname.c_str(), port.c_str());

  run(sfd);

  close(sfd);
  return 0;
}

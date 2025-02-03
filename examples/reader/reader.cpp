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
#define BOLD "\033[1m"  // Bold

#define RED "\033[31m"      // Red
#define GREEN "\033[32m"    // Green
#define YELLOW "\033[33m"   // Yellow
#define BLUE "\033[34m"     // Blue
#define MAGENTA "\033[35m"  // Magenta

bool bold = false;
bool color = false;
bool full = false;
bool space = false;

const char *timestamp() {
  static char time[24];
  struct timeval tv;
  struct tm *tm;

  gettimeofday(&tv, NULL);
  tm = localtime(&tv.tv_sec);

  snprintf(time, sizeof(time), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
           tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);

  return time;
}

std::string collect(const uint8_t byte) {
  static bool running = false;
  static ebus::Sequence sequence;
  std::string result = "";

  if (byte == ebus::sym_syn) {
    if (sequence.size() > 0 && running) {
      ebus::Telegram tel(sequence);
      std::ostringstream ostr;

      if (tel.isValid()) {
        ostr << timestamp();
        ostr << " - ";
        if (color) ostr << GREEN;
        ostr << ebus::Sequence::to_string(tel.getSourceAddress());
        ostr << ebus::Sequence::to_string(tel.getTargetAddress());
        if (color) ostr << RESET;
        if (space) ostr << " ";
        if (color) ostr << BLUE;
        ostr << ebus::Sequence::to_string(tel.getPrimaryCommand());
        ostr << ebus::Sequence::to_string(tel.getSecondaryCommand());
        if (color) ostr << RESET;
        if (space) ostr << " ";
        if (color) ostr << YELLOW;
        ostr << ebus::Sequence::to_string(tel.getMasterNumberBytes());
        if (color) ostr << RESET;
        if (space) ostr << " ";
        if (bold) ostr << BOLD;
        ostr << ebus::Sequence::to_string(tel.getMasterDataBytes());
        if (bold) ostr << RESET;
        if (full) {
          if (space) ostr << " ";
          if (color) ostr << MAGENTA;
          ostr << ebus::Sequence::to_string(tel.getMasterCRC());
          if (color) ostr << RESET;
        } else {
          ostr << "   ";
        }
        if (tel.getType() != ebus::Type::BC) {
          if (full) {
            if (space) ostr << " ";
            ostr << ebus::Sequence::to_string(tel.getSlaveACK());
          } else {
            ostr << "   ";
          }
          if (tel.getType() == ebus::Type::MS) {
            if (space) ostr << " ";
            if (color) ostr << YELLOW;
            ostr << ebus::Sequence::to_string(tel.getSlaveNumberBytes());
            if (color) ostr << RESET;
            if (tel.getSlaveNumberBytes() > 0) {
              if (space) ostr << " ";
              if (bold) ostr << BOLD;
              ostr << ebus::Sequence::to_string(tel.getSlaveDataBytes());
              if (bold) ostr << RESET;
            }
            if (full) {
              if (space) ostr << " ";
              if (color) ostr << MAGENTA;
              ostr << ebus::Sequence::to_string(tel.getSlaveCRC());
              if (color) ostr << RESET;
              if (space) ostr << " ";
              ostr << ebus::Sequence::to_string(tel.getMasterACK());
            }
          }
        }

      } else {
        ostr << timestamp();
        ostr << " - ";
        ostr << sequence.to_string() << std::endl;
        if (color) ostr << RED;
        ostr << "-----ERROR-DETECTED-----> " + tel.to_string();
        if (color) ostr << RESET;
      }

      result = ostr.str();
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
  ssize_t datalen;

  while (1) {
    datalen = recv(sfd, data, sizeof(data), 0);

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
  for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
    sfd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
    if (sfd < 0) {
      err = errno;
      continue;
    }

    if (connect(sfd, addr->ai_addr, addr->ai_addrlen) == 0) {
      break;
    }

    err = errno;
    sfd = -1;
    close(sfd);
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
  std::cout << "  --bold,  -b   data bytes bolded" << std::endl;
  std::cout << "  --color, -c   colorized output" << std::endl;
  std::cout << "  --full,  -f   inclusiv CRC and ACK" << std::endl;
  std::cout << "  --space, -s   space bewtween bytes" << std::endl;
  std::cout << "  --help,  -h   show this page" << std::endl;
}

int main(int argc, char *argv[]) {
  static struct option options[] = {
      {"bold", no_argument, nullptr, 'b'}, {"color", no_argument, nullptr, 'c'},
      {"full", no_argument, nullptr, 'f'}, {"space", no_argument, nullptr, 's'},
      {"help", no_argument, nullptr, 'h'}, {nullptr, 0, nullptr, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "bcfsh", options, nullptr)) != -1) {
    switch (option) {
      case 'b':
        bold = true;
        break;
      case 'c':
        color = true;
        break;
      case 'f':
        full = true;
        break;
      case 's':
        space = true;
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

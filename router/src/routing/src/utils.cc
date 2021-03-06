/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "utils.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>  // sockaddr_in
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

std::pair<std::string, int> get_peer_name(
    const struct sockaddr_storage *addr,
    mysql_harness::SocketOperationsBase *sock_op) {
  std::array<char, 105> result_addr = {0};  // For IPv4, IPv6 and Unix socket

  stdx::expected<const char *, std::error_code> res;
  int port{0};

  if (addr->ss_family == AF_INET6) {
    // IPv6
    auto *sin6 = reinterpret_cast<const struct sockaddr_in6 *>(addr);
    port = ntohs(sin6->sin6_port);
    res = sock_op->inetntop(AF_INET6, &sin6->sin6_addr, result_addr.data(),
                            result_addr.size());
  } else if (addr->ss_family == AF_INET) {
    // IPv4
    const auto *sin4 = reinterpret_cast<const struct sockaddr_in *>(addr);
    port = ntohs(sin4->sin_port);
    res = sock_op->inetntop(AF_INET, &sin4->sin_addr, result_addr.data(),
                            result_addr.size());
  } else if (addr->ss_family == AF_UNIX) {
    // Unix socket, no good way to find peer
    return std::make_pair(std::string("unix socket"), 0);
  } else {
    throw std::runtime_error("unknown address family: " +
                             std::to_string(addr->ss_family));
  }

  if (!res) {
    throw std::system_error(res.error(), "inet_ntop() failed");
  }

  return std::make_pair(std::string(result_addr.data()), port);
}

std::pair<std::string, int> get_peer_name(
    int sock, mysql_harness::SocketOperationsBase *sock_op) {
  struct sockaddr_storage addr;

  size_t sock_len = sizeof addr;
  auto peername_res =
      sock_op->getpeername(sock, (struct sockaddr *)&addr, &sock_len);
  if (!peername_res) {
    throw std::system_error(peername_res.error(), "getpeername() failed");
  }

  return get_peer_name(&addr, sock_op);
}

std::vector<std::string> split_string(const std::string &data,
                                      const char delimiter, bool allow_empty) {
  std::stringstream ss(data);
  std::string token;
  std::vector<std::string> result;

  if (data.empty()) {
    return {};
  }

  while (std::getline(ss, token, delimiter)) {
    if (token.empty() && !allow_empty) {
      // Skip empty
      continue;
    }
    result.push_back(token);
  }

  // When last character is delimiter, it denotes an empty token
  if (allow_empty && data.back() == delimiter) {
    result.push_back("");
  }

  return result;
}

std::vector<std::string> split_string(const std::string &data,
                                      const char delimiter) {
  return split_string(data, delimiter, true);
}

ClientIpArray in_addr_to_array(const sockaddr_storage &addr) {
  ClientIpArray result{{0}};

  switch (addr.ss_family) {
    case AF_INET6: {
      const sockaddr_in6 *addr_intet6 =
          reinterpret_cast<const sockaddr_in6 *>(&addr);
      std::memcpy(result.data(), &addr_intet6->sin6_addr,
                  sizeof(addr_intet6->sin6_addr));
      break;
    }
    default: {
      const sockaddr_in *addr_intet =
          reinterpret_cast<const sockaddr_in *>(&addr);
      std::memcpy(result.data(), &addr_intet->sin_addr,
                  sizeof(addr_intet->sin_addr));
    }
  }

  return result;
}

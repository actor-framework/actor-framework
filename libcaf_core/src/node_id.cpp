/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <cstdio>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>

#include "caf/string_algorithms.hpp"

#include "caf/config.hpp"
#include "caf/node_id.hpp"
#include "caf/serializer.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/get_mac_addresses.hpp"

namespace caf {

namespace {

uint32_t s_invalid_process_id = 0;

node_id::host_id_type s_invalid_host_id;

uint8_t hex_char_value(char c) {
  if (isdigit(c)) {
    return static_cast<uint8_t>(c - '0');
  }
  if (isalpha(c)) {
    if (c >= 'a' && c <= 'f') {
      return static_cast<uint8_t>((c - 'a') + 10);
    } else if (c >= 'A' && c <= 'F') {
      return static_cast<uint8_t>((c - 'A') + 10);
    }
  }
  throw std::invalid_argument(std::string("illegal character: ") + c);
}

void host_id_from_string(const std::string& hash,
                         node_id::host_id_type& node_id) {
  if (hash.size() != (node_id.size() * 2)) {
    throw std::invalid_argument("string argument is not a node id hash");
  }
  auto j = hash.c_str();
  for (size_t i = 0; i < node_id.size(); ++i) {
    // read two characters, each representing 4 bytes
    auto& val = node_id[i];
    val = static_cast<uint8_t>(hex_char_value(*j++) << 4);
    val |= hex_char_value(*j++);
  }
}

} // namespace <anonymous>

bool equal(const std::string& hash, const node_id::host_id_type& node_id) {
  if (hash.size() != (node_id.size() * 2)) {
    return false;
  }
  auto j = hash.c_str();
  try {
    for (size_t i = 0; i < node_id.size(); ++i) {
      // read two characters, each representing 4 bytes
      uint8_t val;
      val = static_cast<uint8_t>(hex_char_value(*j++) << 4);
      val |= hex_char_value(*j++);
      if (val != node_id[i]) {
        return false;
      }
    }
  }
  catch (std::invalid_argument&) {
    return false;
  }
  return true;
}

node_id::~node_id() {
  // nop
}

node_id::node_id(const invalid_node_id_t&) {
  // nop
}

node_id::node_id(const node_id& other) : m_data(other.m_data) {
  // nop
}

node_id::node_id(intrusive_ptr<data> dataptr) : m_data(std::move(dataptr)) {
  // nop
}

node_id::node_id(uint32_t procid, const std::string& b) {
  m_data.reset(new data);
  m_data->process_id = procid;
  host_id_from_string(b, m_data->host_id);
}

node_id::node_id(uint32_t a, const host_id_type& b) : m_data(new data{a, b}) {
  // nop
}

int node_id::compare(const invalid_node_id_t&) const {
  return m_data ? 1 : 0; // invalid instances are always smaller
}

int node_id::compare(const node_id& other) const {
  if (this == &other) {
    return 0; // shortcut for comparing to self
  }
  if (m_data == other.m_data) {
    return 0; // shortcut for identical instances
  }
  if ((m_data != nullptr) != (other.m_data != nullptr)) {
    return m_data ? 1 : -1; // invalid instances are always smaller
  }
  int tmp = strncmp(reinterpret_cast<const char*>(host_id().data()),
                    reinterpret_cast<const char*>(other.host_id().data()),
                    host_id_size);
  if (tmp == 0) {
    if (process_id() < other.process_id()) {
      return -1;
    } else if (process_id() == other.process_id()) {
      return 0;
    }
    return 1;
  }
  return tmp;
}

node_id::data::data(uint32_t procid, host_id_type hid)
    : process_id(procid), host_id(hid) {
  // nop
}

node_id::data::~data() {
  // nop
}

// initializes singleton
node_id::data* node_id::data::create_singleton() {
  auto ifs = detail::get_mac_addresses();
  std::vector<std::string> macs;
  macs.reserve(ifs.size());
  for (auto& i : ifs) {
    macs.emplace_back(std::move(i.ethernet_address));
  }
  auto hd_serial_and_mac_addr = join(macs, "") + detail::get_root_uuid();
  node_id::host_id_type nid;
  detail::ripemd_160(nid, hd_serial_and_mac_addr);
  auto ptr = new node_id::data(static_cast<uint32_t>(getpid()), nid);
  ptr->ref(); // implicit ref count held by detail::singletons
  return ptr;
}

uint32_t node_id::process_id() const {
  return m_data ? m_data->process_id : s_invalid_process_id;
}

const node_id::host_id_type& node_id::host_id() const {
  return m_data ? m_data->host_id : s_invalid_host_id;
}

node_id& node_id::operator=(const invalid_node_id_t&) {
  m_data.reset();
  return *this;
}

} // namespace caf

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/config.hpp"
#include "caf/node_id.hpp"
#include "caf/serializer.hpp"
#include "caf/make_counted.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/get_mac_addresses.hpp"

namespace caf {

namespace {

uint32_t invalid_process_id = 0;

node_id::host_id_type invalid_host_id;

} // namespace <anonymous>

node_id::~node_id() {
  // nop
}

node_id::node_id(const invalid_node_id_t&) {
  // nop
}

node_id& node_id::operator=(const invalid_node_id_t&) {
  data_.reset();
  return *this;
}

node_id::node_id(intrusive_ptr<data> dataptr) : data_(std::move(dataptr)) {
  // nop
}

node_id::node_id(uint32_t procid, const std::string& hash)
    : data_(make_counted<data>(procid, hash)) {
  // nop
}

node_id::node_id(uint32_t procid, const host_id_type& hid)
    : data_(make_counted<data>(procid, hid)) {
  // nop
}

int node_id::compare(const invalid_node_id_t&) const {
  return data_ ? 1 : 0; // invalid instances are always smaller
}

int node_id::compare(const node_id& other) const {
  if (this == &other || data_ == other.data_)
    return 0; // shortcut for comparing to self or identical instances
  if (! data_ != ! other.data_)
    return data_ ? 1 : -1; // invalid instances are always smaller
  int tmp = strncmp(reinterpret_cast<const char*>(host_id().data()),
                    reinterpret_cast<const char*>(other.host_id().data()),
                    host_id_size);
  return tmp != 0
         ? tmp
         : (process_id() < other.process_id()
            ? -1
            : (process_id() == other.process_id() ? 0 : 1));
}

node_id::data::data(uint32_t procid, host_id_type hid)
    : pid_(procid),
      host_(hid) {
  // nop
}

node_id::data::data(uint32_t procid, const std::string& hash) : pid_(procid) {
  if (hash.size() != (host_id_size * 2)) {
    host_ = invalid_host_id;
    return;
  }
  auto hex_value = [](char c) -> uint8_t {
    if (isalpha(c)) {
      if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t>((c - 'a') + 10);
      if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t>((c - 'A') + 10);
    }
    return isdigit(c) ? static_cast<uint8_t>(c - '0') : 0;
  };
  auto j = hash.c_str();
  for (size_t i = 0; i < host_id_size; ++i) {
    // read two characters, each representing 4 bytes
    host_[i] = static_cast<uint8_t>(hex_value(j[0]) << 4) | hex_value(j[1]);
    j += 2;
  }
}

node_id::data::~data() {
  // nop
}

void node_id::data::stop() {
  // nop
}

// initializes singleton
node_id::data* node_id::data::create_singleton() {
  CAF_LOGF_TRACE("");
  auto ifs = detail::get_mac_addresses();
  std::vector<std::string> macs;
  macs.reserve(ifs.size());
  for (auto& i : ifs) {
    macs.emplace_back(std::move(i.second));
  }
  auto hd_serial_and_mac_addr = join(macs, "") + detail::get_root_uuid();
  node_id::host_id_type nid;
  detail::ripemd_160(nid, hd_serial_and_mac_addr);
  // note: pointer has a ref count of 1 -> implicitly held by detail::singletons
  return new node_id::data(detail::get_process_id(), nid);
}

uint32_t node_id::process_id() const {
  return data_ ? data_->pid_ : invalid_process_id;
}

const node_id::host_id_type& node_id::host_id() const {
  return data_ ? data_->host_ : invalid_host_id;
}

} // namespace caf

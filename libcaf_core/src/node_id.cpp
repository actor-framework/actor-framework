/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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
#include <iterator>

#include "caf/config.hpp"
#include "caf/node_id.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/make_counted.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/logger.hpp"
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

node_id::node_id(const none_t&) {
  // nop
}

node_id& node_id::operator=(const none_t&) {
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

int node_id::compare(const none_t&) const {
  return data_ ? 1 : 0; // invalid instances are always smaller
}

int node_id::compare(const node_id& other) const {
  if (this == &other || data_ == other.data_)
    return 0; // shortcut for comparing to self or identical instances
  if (!data_ != !other.data_)
    return data_ ? 1 : -1; // invalid instances are always smaller
  // use mismatch instead of strncmp because the
  // latter bails out on the first 0-byte
  auto last = host_id().end();
  auto ipair = std::mismatch(host_id().begin(), last, other.host_id().begin());
  if (ipair.first == last)
    return static_cast<int>(process_id())-static_cast<int>(other.process_id());
  if (*ipair.first < *ipair.second)
    return -1;
  return 1;
}

node_id::data::data() : pid_(0) {
  memset(host_.data(), 0, host_.size());
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
    if (isalpha(c) != 0) {
      if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t>((c - 'a') + 10);
      if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t>((c - 'A') + 10);
    }
    return isdigit(c) != 0 ? static_cast<uint8_t>(c - '0') : 0;
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

bool node_id::data::valid() const {
  auto is_zero = [](uint8_t x) { return x == 0; };
  return pid_ != 0 && !std::all_of(host_.begin(), host_.end(), is_zero);
}

namespace {

std::atomic<uint8_t> system_id;

} // <anonymous>

// initializes singleton
intrusive_ptr<node_id::data> node_id::data::create_singleton() {
  CAF_LOG_TRACE("");
  auto ifs = detail::get_mac_addresses();
  std::vector<std::string> macs;
  macs.reserve(ifs.size());
  for (auto& i : ifs) {
    macs.emplace_back(std::move(i.second));
  }
  auto hd_serial_and_mac_addr = join(macs, "") + detail::get_root_uuid();
  node_id::host_id_type nid;
  detail::ripemd_160(nid, hd_serial_and_mac_addr);
  // TODO: redesign network layer, make node_id an opaque type, etc.
  // this hack enables multiple actor systems in a single process
  // by overriding the last byte in the node ID with the actor system "ID"
  nid.back() = system_id.fetch_add(1);
  // note: pointer has a ref count of 1 -> implicitly held by detail::singletons
  intrusive_ptr<data> result;
  result.reset(new node_id::data(detail::get_process_id(), nid), false);
  return result;
}

uint32_t node_id::process_id() const {
  return data_ ? data_->pid_ : invalid_process_id;
}

const node_id::host_id_type& node_id::host_id() const {
  return data_ ? data_->host_ : invalid_host_id;
}

node_id::operator bool() const {
  return static_cast<bool>(data_);
}

void node_id::swap(node_id& x) {
  data_.swap(x.data_);
}

std::string to_string(const node_id& x) {
  std::string result;
  append_to_string(result, x);
  return result;
}

void append_to_string(std::string& x, const node_id& y) {
  if (!y) {
    x += "invalid-node";
    return;
  }
  detail::append_hex(x, reinterpret_cast<const uint8_t*>(y.host_id().data()),
                     y.host_id().size());
  x += '#';
  x += std::to_string(y.process_id());
}

} // namespace caf

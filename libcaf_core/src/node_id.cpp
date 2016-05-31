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
  // use mismatch instead of strncmp because the
  // latter bails out on the first 0-byte
  auto last = host_id().end();
  auto ipair = std::mismatch(host_id().begin(), last, other.host_id().begin());
  if (ipair.first == last)
    return static_cast<int>(process_id())-static_cast<int>(other.process_id());
  else if (*ipair.first < *ipair.second)
    return -1;
  else
    return 1;
}

node_id::data::data() : pid_(0) {
  // nop
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

void serialize(serializer& sink, node_id& x, const unsigned int) {
  if (! x.data_) {
    node_id::host_id_type zero_id;
    std::fill(zero_id.begin(), zero_id.end(), 0);
    uint32_t zero_pid = 0;
    sink.apply_raw(zero_id.size(), zero_id.data());
    sink << zero_pid;
  } else {
    sink.apply_raw(x.data_->host_.size(), x.data_->host_.data());
    sink.apply(x.data_->pid_);
  }
}

void serialize(deserializer& source, node_id& x, const unsigned int) {
  node_id::host_id_type tmp_hid;
  uint32_t tmp_pid;
  source.apply_raw(tmp_hid.size(), tmp_hid.data());
  source >> tmp_pid;
  auto is_zero = [](uint8_t value) { return value == 0; };
  // no need to keep the data if it is invalid
  if (tmp_pid == 0 && std::all_of(tmp_hid.begin(), tmp_hid.end(), is_zero)) {
    x = invalid_node_id;
    return;
  }
  if (! x.data_ || ! x.data_->unique()) {
    x.data_ = make_counted<node_id::data>(tmp_pid, tmp_hid);
  } else {
    x.data_->pid_ = tmp_pid;
    x.data_->host_ = tmp_hid;
  }
}

namespace {

inline uint8_t hex_nibble(char c) {
  return static_cast<uint8_t>(c >= '0' && c <= '9'
                              ? c - '0'
                              : (c >= 'a' && c <= 'f' ? (c - 'a') + 10
                                                      : (c - 'A') + 10));
}

} // namespace <anonymous>

void node_id::from_string(const std::string& str) {
  data_.reset();
  if (str == "<invalid-node>")
    return;
  static constexpr size_t hexstr_size = host_id_size * 2;
  // node id format is: "[0-9a-zA-Z]{40}:[0-9]+"
  if (str.size() < hexstr_size + 2)
    return;
  auto beg = str.begin();
  auto sep = beg + hexstr_size; // separator ':' / end of hex-string
  auto eos = str.end(); // end-of-string
  if (*sep != ':')
    return;
  if (! std::all_of(beg, sep, ::isxdigit))
    return;
  if (! std::all_of(sep + 1, eos, ::isdigit))
    return;
  // iterate two digits in the input string as one byte in hex format
  struct hex_byte_iter : std::iterator<std::input_iterator_tag, uint8_t> {
    using const_iterator = std::string::const_iterator;
    const_iterator i;
    hex_byte_iter(const_iterator x) : i(x) {
      // nop
    }
    uint8_t operator*() const {
      return static_cast<uint8_t>(hex_nibble(*i) << 4) | hex_nibble(*(i + 1));
    }
    hex_byte_iter& operator++() {
      i += 2;
      return *this;
    }
    bool operator!=(const hex_byte_iter& x) const {
      return i != x.i;
    }
  };
  hex_byte_iter first{beg};
  hex_byte_iter last{sep};
  data_.reset(new data);
  std::copy(first, last, data_->host_.begin());
  data_->pid_ = static_cast<uint32_t>(atoll(&*(sep + 1)));
}

std::string to_string(const node_id& x) {
  if (! x)
    return "<invalid-node>";
  std::ostringstream oss;
  oss << std::hex;
  oss.fill('0');
  auto& hid = x.host_id();
  for (auto val : hid) {
    oss.width(2);
    oss << static_cast<uint32_t>(val);
  }
  oss << ":" << std::dec << x.process_id();
  return oss.str();
}

} // namespace caf

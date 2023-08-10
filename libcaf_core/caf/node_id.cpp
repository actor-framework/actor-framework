// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/node_id.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/config.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/ascii_to_int.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/parser_state.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"
#include "caf/string_algorithms.hpp"

#include <ctype.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <random>
#include <sstream>
#include <string_view>
#include <tuple>

namespace {

std::atomic<uint8_t> system_id;

} // namespace

namespace caf {

hashed_node_id::hashed_node_id() noexcept : process_id(0) {
  memset(host.data(), 0, host.size());
}

hashed_node_id::hashed_node_id(uint32_t pid, const host_id_type& host) noexcept
  : process_id(pid), host(host) {
  // nop
}

bool hashed_node_id::valid() const noexcept {
  return process_id != 0 && valid(host);
}

int hashed_node_id::compare(const hashed_node_id& other) const noexcept {
  if (this == &other)
    return 0;
  if (process_id != other.process_id)
    return process_id < other.process_id ? -1 : 1;
  return memcmp(host.data(), other.host.data(), host.size());
}

void hashed_node_id::print(std::string& dst) const {
  if (!valid()) {
    dst += "invalid-node";
    return;
  }
  detail::append_hex(dst, host);
  dst += '#';
  dst += std::to_string(process_id);
}

bool hashed_node_id::valid(const host_id_type& x) noexcept {
  auto is_zero = [](uint8_t x) { return x == 0; };
  return !std::all_of(x.begin(), x.end(), is_zero);
}

bool hashed_node_id::can_parse(std::string_view str) noexcept {
  // Our format is "<20-byte-hex>#<pid>". With 2 characters per byte, this means
  // a valid node ID has at least 42 characters.
  if (str.size() < 42)
    return false;
  string_parser_state ps{str.begin(), str.end()};
  for (size_t i = 0; i < 40; ++i)
    if (!ps.consume_strict_if(isxdigit))
      return false;
  if (!ps.consume_strict('#'))
    return false;
  // We don't care for the value, but we invoke the actual number parser to make
  // sure the value is in bounds.
  uint32_t dummy;
  detail::parse(ps, dummy);
  return ps.code == pec::success;
}

node_id hashed_node_id::local(const actor_system_config&) {
  CAF_LOG_TRACE("");
  // We add an global incrementing counter to make sure two actor systems in the
  // same process won't have the same node ID - even if the user manipulates the
  // system to always produce the same seed for its randomness.
  auto sys_seed = static_cast<uint8_t>(system_id.fetch_add(1));
  std::random_device rd;
  std::minstd_rand gen{rd() + sys_seed};
  // uniform_int_distribution doesn't accept uint8_t as template parameter
  std::uniform_int_distribution<> dis(std::numeric_limits<uint8_t>::min(),
                                      std::numeric_limits<uint8_t>::max());
  host_id_type hid;
  for (auto& x : hid)
    x = static_cast<uint8_t>(dis(gen));
  return make_node_id(detail::get_process_id(), hid);
}

node_id_data::~node_id_data() {
  // nop
}

node_id& node_id::operator=(const none_t&) {
  data_.reset();
  return *this;
}

int node_id::compare(const node_id& other) const noexcept {
  struct {
    int operator()(const uri&, const hashed_node_id&) const noexcept {
      return -1;
    }
    int operator()(const hashed_node_id&, const uri&) const noexcept {
      return 1;
    }
    int operator()(const uri& x, const uri& y) const noexcept {
      return x.compare(y);
    }
    int operator()(const hashed_node_id& x,
                   const hashed_node_id& y) const noexcept {
      return x.compare(y);
    }
  } comparator;
  if (this == &other || data_ == other.data_)
    return 0;
  if (data_ == nullptr)
    return other.data_ == nullptr ? 0 : -1;
  return other.data_ == nullptr
           ? 1
           : visit(comparator, data_->content, other.data_->content);
}

void node_id::swap(node_id& x) noexcept {
  data_.swap(x.data_);
}

bool node_id::can_parse(std::string_view str) noexcept {
  return default_data::can_parse(str) || uri::can_parse(str);
}

void append_to_string(std::string& str, const node_id& x) {
  auto f = [&str](auto& x) {
    if constexpr (std::is_same_v<std::decay_t<decltype(x)>, uri>)
      str.insert(str.end(), x.str().begin(), x.str().end());
    else
      x.print(str);
  };
  if (x)
    visit(f, x->content);
  else
    str += "invalid-node";
}

std::string to_string(const node_id& x) {
  std::string result;
  append_to_string(result, x);
  return result;
}

node_id make_node_id(uri from) {
  return node_id{std::move(from)};
}

node_id make_node_id(uint32_t process_id,
                     const hashed_node_id::host_id_type& host_id) {
  return node_id{node_id::default_data{process_id, host_id}};
}

std::optional<node_id> make_node_id(uint32_t process_id,
                                    std::string_view host_hash) {
  using id_type = hashed_node_id::host_id_type;
  if (host_hash.size() != std::tuple_size_v<id_type> * 2)
    return std::nullopt;
  detail::parser::ascii_to_int<16, uint8_t> xvalue;
  id_type host_id;
  auto in = host_hash.begin();
  for (auto& byte : host_id) {
    if (!isxdigit(*in))
      return std::nullopt;
    auto first_nibble = (xvalue(*in++) << 4);
    if (!isxdigit(*in))
      return std::nullopt;
    byte = static_cast<uint8_t>(first_nibble | xvalue(*in++));
  }
  if (!hashed_node_id::valid(host_id))
    return std::nullopt;
  return make_node_id(process_id, host_id);
}

error parse(std::string_view str, node_id& dest) {
  if (node_id::default_data::can_parse(str)) {
    CAF_ASSERT(str.size() >= 42);
    auto host_hash = str.substr(0, 40);
    auto pid_str = str.substr(41);
    uint32_t pid_val = 0;
    if (auto err = detail::parse(pid_str, pid_val))
      return err;
    if (auto nid = make_node_id(pid_val, host_hash)) {
      dest = std::move(*nid);
      return none;
    }
    CAF_LOG_ERROR("make_node_id failed after can_parse returned true");
    return sec::invalid_argument;
  }
  if (auto nid_uri = make_uri(str)) {
    dest = make_node_id(std::move(*nid_uri));
    return none;
  } else {
    return std::move(nid_uri.error());
  }
}

} // namespace caf

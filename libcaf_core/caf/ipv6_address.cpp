// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/ipv6_address.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/parser/read_ipv6_address.hpp"
#include "caf/error.hpp"
#include "caf/format_to_error.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/message.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"

#include <cstring>
#include <string_view>

namespace caf {

namespace {

struct ipv6_address_consumer {
  ipv6_address& dest;

  ipv6_address_consumer(ipv6_address& ref) : dest(ref) {
    // nop
  }

  void value(ipv6_address val) {
    dest = val;
  }
};

// It's good practice when printing IPv6 addresses to:
// - remove leading zeros in all segments
// - use lowercase hexadecimal characters
// @param pointer to an uint16_t segment converted to uint8_t*
void append_v6_hex(std::string& result, const uint8_t* xs) {
  auto tbl = "0123456789abcdef";
  size_t j = 0;
  std::array<char, (sizeof(uint16_t) * 2) + 1> buf;
  buf.fill(0);
  for (size_t i = 0; i < sizeof(uint16_t); ++i) {
    auto c = xs[i];
    buf[j++] = tbl[c >> 4];
    buf[j++] = tbl[c & 0x0F];
  }
  auto pred = [](char c) { return c != '0'; };
  auto first_non_zero = std::find_if(buf.begin(), buf.end(), pred);
  CAF_ASSERT(first_non_zero != buf.end());
  if (*first_non_zero != '\0')
    result += &(*first_non_zero);
  else
    result += '0';
}

inline uint32_t net_order_32(uint32_t value) {
  return detail::to_network_order(value);
}

inline uint64_t net_order_64(uint64_t value) {
  return detail::to_network_order(value);
}

std::array<uint32_t, 3> v4_prefix{{0, 0, net_order_32(0x0000FFFFu)}};

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

ipv6_address::ipv6_address() {
  half_segments_[0] = 0;
  half_segments_[1] = 0;
}

ipv6_address::ipv6_address(uint16_ilist prefix, uint16_ilist suffix) {
  CAF_ASSERT((prefix.size() + suffix.size()) <= 8);
  auto addr_fill = [&](uint16_ilist chunks, size_t p) {
    union {
      uint16_t i;
      std::array<uint8_t, 2> a;
    } tmp;
    for (auto chunk : chunks) {
      tmp.i = detail::to_network_order(chunk);
      bytes_[p++] = tmp.a[0];
      bytes_[p++] = tmp.a[1];
    }
  };
  bytes_.fill(0);
  addr_fill(prefix, 0);
  addr_fill(suffix, num_bytes - (suffix.size() * 2));
}

ipv6_address::ipv6_address(ipv4_address addr) {
  std::copy(v4_prefix.begin(), v4_prefix.end(), quad_segments_.begin());
  quad_segments_.back() = addr.bits();
}

ipv6_address::ipv6_address(array_type bytes) {
  memcpy(bytes_.data(), bytes.data(), bytes.size());
}

// -- comparison ---------------------------------------------------------------

int ipv6_address::compare(ipv6_address other) const noexcept {
  return memcmp(bytes().data(), other.bytes().data(), num_bytes);
}

int ipv6_address::compare(ipv4_address other) const noexcept {
  ipv6_address tmp{other};
  return compare(tmp);
}

// -- properties ---------------------------------------------------------------

bool ipv6_address::embeds_v4() const noexcept {
  using std::begin;
  using std::end;
  return std::equal(begin(v4_prefix), end(v4_prefix), quad_segments_.begin());
}

ipv4_address ipv6_address::embedded_v4() const noexcept {
  ipv4_address result;
  result.bits(quad_segments_.back());
  return result;
}

bool ipv6_address::is_loopback() const noexcept {
  // IPv6 defines "::1" as the loopback address, when embedding a v4 we
  // dispatch accordingly.
  return embeds_v4()
           ? embedded_v4().is_loopback()
           : half_segments_[0] == 0 && half_segments_[1] == net_order_64(1u);
}

// -- factories ----------------------------------------------------------------

ipv6_address ipv6_address::any() noexcept {
  return ip_address{{0}, {0}};
}

ipv6_address ipv6_address::loopback() noexcept {
  return ip_address{{0}, {0x1}};
}

// -- related free functions ---------------------------------------------------

namespace {

using u16_iterator = ipv6_address::u16_array_type::const_iterator;

using u16_range = std::pair<u16_iterator, u16_iterator>;

u16_range longest_streak(u16_iterator first, u16_iterator last) {
  auto two_zeros = [](uint16_t x, uint16_t y) { return x == 0 && y == 0; };
  auto not_zero = [](uint16_t x) { return x != 0; };
  u16_range result;
  result.first = std::adjacent_find(first, last, two_zeros);
  if (result.first == last)
    return {last, last};
  result.second = std::find_if(result.first + 2, last, not_zero);
  if (result.second == last)
    return result;
  auto next_streak = longest_streak(result.second, last);
  auto range_size = [](u16_range x) {
    return std::distance(x.first, x.second);
  };
  return range_size(result) >= range_size(next_streak) ? result : next_streak;
}

} // namespace

std::string to_string(ipv6_address x) {
  // Shortcut for embedded v4 addresses.
  if (x.embeds_v4())
    return to_string(x.embedded_v4());
  // Shortcut for all-zero addresses.
  if (x.zero())
    return "::";
  // Output buffer.
  std::string result;
  // Utility for appending chunks to the result.
  auto add_chunk = [&](uint16_t chunk) {
    append_v6_hex(result, reinterpret_cast<uint8_t*>(&chunk));
  };
  auto add_chunks = [&](u16_iterator i, u16_iterator e) {
    if (i != e) {
      add_chunk(*i);
      for (++i; i != e; ++i) {
        result += ':';
        add_chunk(*i);
      }
    }
  };
  // Scan for the longest streak of 0 16-bit chunks for :: shorthand.
  auto first = x.oct_segments_.cbegin();
  auto last = x.oct_segments_.cend();
  auto streak = longest_streak(first, last);
  // Put it all together.
  if (streak.first == last) {
    add_chunks(first, last);
  } else {
    add_chunks(first, streak.first);
    result += "::";
    add_chunks(streak.second, last);
  }
  return result;
}

error parse(std::string_view str, ipv6_address& dest) {
  using namespace detail;
  ipv6_address_consumer f{dest};
  string_parser_state res{str.begin(), str.end()};
  parser::read_ipv6_address(res, f);
  if (res.code == pec::success)
    return none;
  return format_to_error(res.code, "invalid syntax in line {} column {}",
                         res.line, res.column);
}

} // namespace caf

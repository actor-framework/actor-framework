// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv4_address.hpp"

#include "caf/detail/parser/read_ipv4_address.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"

#include <string_view>

namespace caf {

namespace {

struct ipv4_address_consumer {
  ipv4_address& dest;

  explicit ipv4_address_consumer(ipv4_address& ref) : dest(ref) {
    // nop
  }

  void value(ipv4_address val) {
    dest = val;
  }
};

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

ipv4_address::ipv4_address() = default;

ipv4_address::ipv4_address(array_type bytes) : bytes_(bytes) {
  // nop
}

// -- properties ---------------------------------------------------------------

bool ipv4_address::is_loopback() const noexcept {
  // All addresses in 127.0.0.0/8 are considered loopback addresses.
  return bytes_[0] == 127;
}

bool ipv4_address::is_multicast() const noexcept {
  // All addresses in 224.0.0.0/4 are considered multicast addresses.
  return (bytes_[0] & 0xF0) == 0xE0;
}

// -- factories ----------------------------------------------------------------

ipv4_address ipv4_address::any() noexcept {
  return make_ipv4_address(0, 0, 0, 0);
}

ipv4_address ipv4_address::loopback() noexcept {
  return make_ipv4_address(127, 0, 0, 1);
}

// -- related free functions ---------------------------------------------------

ipv4_address make_ipv4_address(uint8_t oct1, uint8_t oct2, uint8_t oct3,
                               uint8_t oct4) {
  ipv4_address::array_type bytes{{oct1, oct2, oct3, oct4}};
  return ipv4_address{bytes};
}

std::string to_string(const ipv4_address& x) {
  using std::to_string;
  std::string result;
  result += to_string(x[0]);
  for (size_t i = 1; i < x.data().size(); ++i) {
    result += '.';
    result += to_string(x[i]);
  }
  return result;
}

error parse(std::string_view str, ipv4_address& dest) {
  using namespace detail;
  ipv4_address_consumer f{dest};
  string_parser_state res{str.begin(), str.end()};
  parser::read_ipv4_address(res, f);
  if (res.code == pec::success)
    return none;
  return res.error();
}

} // namespace caf

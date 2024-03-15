// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/protocol.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"

#include <algorithm>
#include <string_view>

using namespace caf;
using namespace std::literals;

using caf::io::network::protocol;

namespace {

TEST("inspection") {
  auto roundtrip = [this](auto x) {
    using value_type = decltype(x);
    byte_buffer buf;
    binary_serializer sink{buf};
    if (!sink.apply(x))
      fail("serialization failed: {}", to_string(sink.get_error()));
    binary_deserializer source{buf};
    auto y = value_type{};
    if (!source.apply(y))
      fail("deserialization failed: {}", to_string(source.get_error()));
    return y;
  };
  auto ipv4_tcp = protocol{protocol::tcp, protocol::ipv4};
  auto ipv4_udp = protocol{protocol::udp, protocol::ipv4};
  auto ipv6_tcp = protocol{protocol::tcp, protocol::ipv6};
  auto ipv6_udp = protocol{protocol::udp, protocol::ipv6};
  check_ne(ipv4_tcp, ipv4_udp);
  check_ne(ipv4_tcp, ipv6_tcp);
  check_ne(ipv4_tcp, ipv6_udp);
  check_eq(ipv4_tcp, roundtrip(ipv4_tcp));
  check_eq(ipv4_udp, roundtrip(ipv4_udp));
  check_eq(ipv6_tcp, roundtrip(ipv6_tcp));
  check_eq(ipv6_udp, roundtrip(ipv6_udp));
}

TEST("string conversion") {
  auto ipv4_tcp = protocol{protocol::tcp, protocol::ipv4};
  auto ipv4_udp = protocol{protocol::udp, protocol::ipv4};
  auto ipv6_tcp = protocol{protocol::tcp, protocol::ipv6};
  auto ipv6_udp = protocol{protocol::udp, protocol::ipv6};
  check_eq(to_string(ipv4_tcp), "TCP/IPv4");
  check_eq(to_string(ipv4_udp), "UDP/IPv4");
  check_eq(to_string(ipv6_tcp), "TCP/IPv6");
  check_eq(to_string(ipv6_udp), "UDP/IPv6");
}

} // namespace

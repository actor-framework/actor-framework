// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/ip_endpoint.hpp"

#include "caf/test/test.hpp"

#include "caf/io/network/interfaces.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"

#include <functional>

using namespace caf;
using namespace std::literals;

using io::network::interfaces;

namespace {

TEST("IP endpoints are empty by default, copyable and movable") {
  auto addr_int = [](const void* ptr) {
    return reinterpret_cast<intptr_t>(ptr);
  };
  auto uut = io::network::ip_endpoint{};
  check_eq(to_string(uut), ":0");
  check_eq(*uut.length(), 0u);
  check_eq(*uut.clength(), 0u);
  check_ne(addr_int(uut.address()), 0);
  check_ne(addr_int(uut.caddress()), 0);
  check(!is_ipv4(uut));
  check(!is_ipv6(uut));
  SECTION("IP endpoints are copyable") {
    auto cpy = uut;
    check_eq(*cpy.length(), 0u);
    check_eq(*cpy.clength(), 0u);
    check_eq(uut, cpy);
    check_ne(addr_int(cpy.address()), addr_int(uut.address()));
  }
  SECTION("IP endpoints are movable") {
    auto cpy = uut;
    auto cpy_addr = cpy.address();
    auto mv = std::move(cpy);
    check_eq(addr_int(mv.address()), addr_int(cpy_addr));
  }
}

// Note: interfaces::get_endpoint seems currently broken on Windows. This
// affects UDP brokers, which are experimental and will probably be removed
// anyways in favor of caf_net primitives.
#ifndef CAF_WINDOWS
TEST("IP endpoints are convertible from string plus port") {
  auto v4_1 = io::network::ip_endpoint{};
  auto v4_2 = io::network::ip_endpoint{};
  auto v6_1 = io::network::ip_endpoint{};
  auto v6_2 = io::network::ip_endpoint{};
  check(interfaces::get_endpoint("192.168.9.1", 1234, v4_1));
  check(interfaces::get_endpoint("192.168.9.2", 2345, v4_2));
  check(interfaces::get_endpoint("fe80::abcd", 1234, v6_1));
  check(interfaces::get_endpoint("fe80::bcde", 2345, v6_2));
  SECTION("different endpoint do not compare equal") {
    check_ne(v4_1, v4_2);
    check_ne(v6_1, v6_2);
    check_ne(v4_1, v6_1);
    check_ne(v4_2, v6_2);
  }
  SECTION("IPv4 addresses result in IPv4 endpoints") {
    check(is_ipv4(v4_1));
    check(is_ipv4(v4_2));
    check(!is_ipv4(v6_1));
    check(!is_ipv4(v6_2));
  }
  SECTION("IPv6 addresses result in IPv6 endpoints") {
    check(!is_ipv6(v4_1));
    check(!is_ipv6(v4_2));
    check(is_ipv6(v6_1));
    check(is_ipv6(v6_2));
  }
  SECTION("IP endpoints are convertible to string") {
    check_eq(to_string(v4_1), "192.168.9.1:1234");
    check_eq(to_string(v4_2), "192.168.9.2:2345");
    check_eq(to_string(v6_1), "[fe80::abcd]:1234");
    check_eq(to_string(v6_2), "[fe80::bcde]:2345");
  }
  SECTION("IP endpoints are inspectable") {
    auto roundtrip = [this](const auto& x) {
      using value_type = std::decay_t<decltype(x)>;
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
    check_eq(roundtrip(v4_1), v4_1);
    check_eq(roundtrip(v4_2), v4_2);
    check_eq(roundtrip(v6_1), v6_1);
    check_eq(roundtrip(v6_2), v6_2);
  }
  SECTION("IP endpoints are hashable") {
    std::hash<caf::io::network::ip_endpoint> hash;
    check_eq(hash(v4_1), hash(v4_1));
    check_ne(hash(v4_1), hash(v4_2));
    check_eq(hash(v6_1), hash(v6_1));
    check_ne(hash(v6_1), hash(v6_2));
    check_ne(hash(v4_1), hash(v6_1));
    check_ne(hash(v4_2), hash(v6_2));
  }
}
#endif

} // namespace

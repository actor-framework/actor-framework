// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.ripemd_160

#include "caf/detail/ripemd_160.hpp"

#include "core-test.hpp"

#include <iomanip>
#include <iostream>

using caf::detail::ripemd_160;

namespace {

std::string str_hash(const std::string& what) {
  std::array<uint8_t, 20> hash;
  ripemd_160(hash, what);
  std::ostringstream oss;
  oss << std::setfill('0') << std::hex;
  for (auto i : hash) {
    oss << std::setw(2) << static_cast<int>(i);
  }
  return oss.str();
}

} // namespace

// verify ripemd implementation with example hash results from
// http://homes.esat.kuleuven.be/~bosselae/ripemd160.html
CAF_TEST(hash_results) {
  CHECK_EQ("9c1185a5c5e9fc54612808977ee8f548b2258d31", str_hash(""));
  CHECK_EQ("0bdc9d2d256b3ee9daae347be6f4dc835a467ffe", str_hash("a"));
  CHECK_EQ("8eb208f7e05d987a9b044a8e98c6b087f15a0bfc", str_hash("abc"));
  CHECK_EQ("5d0689ef49d2fae572b881b123a85ffa21595f36",
           str_hash("message digest"));
  CHECK_EQ("f71c27109c692c1b56bbdceb5b9d2865b3708dbc",
           str_hash("abcdefghijklmnopqrstuvwxyz"));
  CHECK_EQ("12a053384a9c0c88e405a06c27dcf49ada62eb2b",
           str_hash("abcdbcdecdefdefgefghfghighij"
                    "hijkijkljklmklmnlmnomnopnopq"));
  CHECK_EQ("b0e20b6e3116640286ed3a87a5713079b21f5189",
           str_hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"
                    "fghijklmnopqrstuvwxyz0123456789"));
  CHECK_EQ("9b752e45573d4b39f4dbd3323cab82bf63326bfb",
           str_hash("1234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890"));
}

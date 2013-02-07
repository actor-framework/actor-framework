#include <sstream>
#include <iostream>

#include "test.hpp"

#include "cppa/util/ripemd_160.hpp"

using cppa::util::ripemd_160;

namespace {

std::string str_hash(const std::string& what) {
    std::array<std::uint8_t, 20> hash;
    ripemd_160(hash, what);
    std::ostringstream oss;
    oss << std::hex;
    for (auto i : hash) {
        oss.width(2);
        oss.fill('0');
        oss << static_cast<std::uint32_t>(i);
    }
    return oss.str();
}

} // namespace <anonymous>

// verify ripemd implementation with example hash results from
// http://homes.esat.kuleuven.be/~bosselae/ripemd160.html
int main() {
    CPPA_TEST(test__ripemd_160);

    CPPA_CHECK_EQUAL("9c1185a5c5e9fc54612808977ee8f548b2258d31",
                     str_hash(""));

    CPPA_CHECK_EQUAL("0bdc9d2d256b3ee9daae347be6f4dc835a467ffe",
                     str_hash("a"));

    CPPA_CHECK_EQUAL("8eb208f7e05d987a9b044a8e98c6b087f15a0bfc",
                     str_hash("abc"));

    CPPA_CHECK_EQUAL("5d0689ef49d2fae572b881b123a85ffa21595f36",
                     str_hash("message digest"));

    CPPA_CHECK_EQUAL("f71c27109c692c1b56bbdceb5b9d2865b3708dbc",
                     str_hash("abcdefghijklmnopqrstuvwxyz"));

    CPPA_CHECK_EQUAL("12a053384a9c0c88e405a06c27dcf49ada62eb2b",
                     str_hash("abcdbcdecdefdefgefghfghighij"
                              "hijkijkljklmklmnlmnomnopnopq"));

    CPPA_CHECK_EQUAL("b0e20b6e3116640286ed3a87a5713079b21f5189",
                     str_hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"
                              "fghijklmnopqrstuvwxyz0123456789"));

    CPPA_CHECK_EQUAL("9b752e45573d4b39f4dbd3323cab82bf63326bfb",
                     str_hash("1234567890123456789012345678901234567890"
                              "1234567890123456789012345678901234567890"));

    return CPPA_TEST_RESULT();
}

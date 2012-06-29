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
size_t test__ripemd_160() {
    CPPA_TEST(test__ripemd_160);
    CPPA_CHECK_EQUAL(str_hash(""),
                     "9c1185a5c5e9fc54612808977ee8f548b2258d31");
    CPPA_CHECK_EQUAL(str_hash("a"),
                     "0bdc9d2d256b3ee9daae347be6f4dc835a467ffe");
    CPPA_CHECK_EQUAL(str_hash("abc"),
                     "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc");
    CPPA_CHECK_EQUAL(str_hash("message digest"),
                     "5d0689ef49d2fae572b881b123a85ffa21595f36");
    CPPA_CHECK_EQUAL(str_hash("abcdefghijklmnopqrstuvwxyz"),
                     "f71c27109c692c1b56bbdceb5b9d2865b3708dbc");
    CPPA_CHECK_EQUAL(str_hash("abcdbcdecdefdefgefghfghighij"
                              "hijkijkljklmklmnlmnomnopnopq"),
                     "12a053384a9c0c88e405a06c27dcf49ada62eb2b");
    CPPA_CHECK_EQUAL(str_hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"
                              "fghijklmnopqrstuvwxyz0123456789"),
                     "b0e20b6e3116640286ed3a87a5713079b21f5189");
    CPPA_CHECK_EQUAL(str_hash("1234567890123456789012345678901234567890"
                              "1234567890123456789012345678901234567890"),
                     "9b752e45573d4b39f4dbd3323cab82bf63326bfb");
    //CPPA_CHECK_EQUAL(str_hash(std::string(1000000, 'a')),
    //                 "52783243c1697bdbe16d37f97f68f08325dc1528");
    return CPPA_TEST_RESULT;
}

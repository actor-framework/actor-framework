#include "cppa/atom.hpp"

namespace cppa {

std::string to_string(const atom_value& a)
{
    std::string result;
    result.reserve(11);
    for (auto x = static_cast<std::uint64_t>(a); x != 0; x >>= 6)
    {
        // decode 6 bit characters to ASCII
        result += detail::decoding_table[static_cast<size_t>(x & 0x3F)];
    }
    // result is reversed, since we read from right-to-left
    return std::string(result.rbegin(), result.rend());
}

} // namespace cppa

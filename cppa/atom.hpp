#ifndef ATOM_HPP
#define ATOM_HPP

#include <string>

#include "cppa/detail/atom_val.hpp"

namespace cppa {

enum class atom_value : std::uint64_t { dirty_little_hack = 37337 };

std::string to_string(const atom_value& a);

template<size_t Size>
constexpr atom_value atom(const char (&str) [Size])
{
    // last character is the NULL terminator
    static_assert(Size <= 11, "only 10 characters are allowed");
    return static_cast<atom_value>(detail::atom_val(str, 0));
}

} // namespace cppa

#endif // ATOM_HPP

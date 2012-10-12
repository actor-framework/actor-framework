/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <limits>
#include "cppa/detail/demangle.hpp"
#include "cppa/detail/decorated_names_map.hpp"

using namespace std;

namespace {

using namespace cppa;
using namespace cppa::detail;

constexpr const char* mapped_int_names[][2] = {
    { nullptr, nullptr }, // sizeof 0-> invalid
    { "@i8",   "@u8"   }, // sizeof 1 -> signed / unsigned int8
    { "@i16",  "@u16"  }, // sizeof 2 -> signed / unsigned int16
    { nullptr, nullptr }, // sizeof 3-> invalid
    { "@i32",  "@u32"  }, // sizeof 4 -> signed / unsigned int32
    { nullptr, nullptr }, // sizeof 5-> invalid
    { nullptr, nullptr }, // sizeof 6-> invalid
    { nullptr, nullptr }, // sizeof 7-> invalid
    { "@i64",  "@u64"  }  // sizeof 8 -> signed / unsigned int64
};

template<typename T>
constexpr size_t sign_index() {
    static_assert(numeric_limits<T>::is_integer, "T is not an integer");
    return numeric_limits<T>::is_signed ? 0 : 1;
}

template<typename T>
inline string demangled() {
    return demangle(typeid(T));
}

template<typename T>
constexpr const char* mapped_int_name() {
    return mapped_int_names[sizeof(T)][sign_index<T>()];
}

template<typename T>
struct is_signed : integral_constant<bool, numeric_limits<T>::is_signed> { };

} // namespace <anonymous>

namespace cppa { namespace detail {

decorated_names_map::decorated_names_map() {
    map<string, string> tmp = {
        // integer types
        { demangled<char>(), mapped_int_name<char>() },
        { demangled<signed char>(), mapped_int_name<signed char>() },
        { demangled<unsigned char>(), mapped_int_name<unsigned char>() },
        { demangled<short>(), mapped_int_name<short>() },
        { demangled<signed short>(), mapped_int_name<signed short>() },
        { demangled<unsigned short>(), mapped_int_name<unsigned short>() },
        { demangled<short int>(), mapped_int_name<short int>() },
        { demangled<signed short int>(), mapped_int_name<signed short int>() },
        { demangled<unsigned short int>(), mapped_int_name<unsigned short int>()},
        { demangled<int>(), mapped_int_name<int>() },
        { demangled<signed int>(), mapped_int_name<signed int>() },
        { demangled<unsigned int>(), mapped_int_name<unsigned int>() },
        { demangled<long int>(), mapped_int_name<long int>() },
        { demangled<signed long int>(), mapped_int_name<signed long int>() },
        { demangled<unsigned long int>(), mapped_int_name<unsigned long int>() },
        { demangled<long>(), mapped_int_name<long>() },
        { demangled<signed long>(), mapped_int_name<signed long>() },
        { demangled<unsigned long>(), mapped_int_name<unsigned long>() },
        { demangled<long long>(), mapped_int_name<long long>() },
        { demangled<signed long long>(), mapped_int_name<signed long long>() },
        { demangled<unsigned long long>(), mapped_int_name<unsigned long long>()},
        { demangled<char16_t>(), mapped_int_name<char16_t>() },
        { demangled<char32_t>(), mapped_int_name<char32_t>() },
        { "cppa::atom_value", "@atom" },
        { "cppa::any_tuple",  "@<>" },
        { "cppa::network::addressed_message", "@msg" },
        { "cppa::intrusive_ptr<cppa::actor>", "@actor" },
        { "cppa::intrusive_ptr<cppa::group>" ,"@group" },
        { "cppa::intrusive_ptr<cppa::channel>", "@channel" },
        { "cppa::intrusive_ptr<cppa::process_information>", "@process_info" },
        { "std::basic_string<@i8,std::char_traits<@i8>,std::allocator<@i8>>", "@str" },
        { "std::basic_string<@u16,std::char_traits<@u16>,std::allocator<@u16>>", "@u16str" },
        { "std::basic_string<@u32,std::char_traits<@u32>,std::allocator<@u32>>", "@u32str"},
        { "std::map<@str,@str,std::less<@str>,std::allocator<std::pair<const @str,@str>>>", "@strmap" },
        { "std::string", "@str" }, // GCC
        { "cppa::util::void_type", "@0" }
    };
    m_map = move(tmp);
}

const string& decorated_names_map::decorate(const string& what) const {
    auto i = m_map.find(what);
    return (i != m_map.end()) ? i->second : what;
}


} } // namespace cppa::detail

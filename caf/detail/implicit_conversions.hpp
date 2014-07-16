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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP
#define CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP

#include <string>
#include <type_traits>

#include "caf/detail/type_traits.hpp"

namespace caf {
class local_actor;
} // namespace caf

namespace caf {
namespace detail {

template<typename T>
struct implicit_conversions {

    using subtype1 = typename replace_type<
                         T,
                         std::string,
                         std::is_same<T, const char*>,
                         std::is_same<T, char*>,
                         std::is_same<T, char[]>,
                         is_array_of<T, char>,
                         is_array_of<T, const char>
                     >::type;

    using subtype2 = typename replace_type<
                         subtype1,
                         std::u16string,
                         std::is_same<subtype1, const char16_t*>,
                         std::is_same<subtype1, char16_t*>,
                         is_array_of<subtype1, char16_t>
                     >::type;

    using subtype3 = typename replace_type<
                         subtype2,
                         std::u32string,
                         std::is_same<subtype2, const char32_t*>,
                         std::is_same<subtype2, char32_t*>,
                         is_array_of<subtype2, char32_t>
                     >::type;

    using type = typename replace_type<
                     subtype3,
                     actor,
                     std::is_convertible<T, abstract_actor*>,
                     std::is_same<scoped_actor, T>
                 >::type;

};

template<typename T>
struct strip_and_convert {
    using type = typename implicit_conversions<
                     typename rm_const_and_ref<T>::type
                 >::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP

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


#ifndef CPPA_WILDCARD_POSITION_HPP
#define CPPA_WILDCARD_POSITION_HPP

#include <type_traits>

#include "cppa/anything.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

/**
 * @brief Denotes the position of {@link cppa::anything anything} in a
 *        template parameter pack.
 */
enum class wildcard_position {
    nil,
    trailing,
    leading,
    in_between,
    multiple
};

/**
 * @brief Gets the position of {@link cppa::anything anything} from the
 *        type list @p Types.
 * @tparam A template parameter pack as {@link cppa::util::type_list type_list}.
 */
template<typename Types>
constexpr wildcard_position get_wildcard_position() {
    return util::tl_count<Types, is_anything>::value > 1
           ? wildcard_position::multiple
           : (util::tl_count<Types, is_anything>::value == 1
              ? (std::is_same<typename util::tl_head<Types>::type, anything>::value
                 ? wildcard_position::leading
                 : (std::is_same<typename util::tl_back<Types>::type, anything>::value
                    ? wildcard_position::trailing
                    : wildcard_position::in_between))
              : wildcard_position::nil);
}

} // namespace cppa

#endif // CPPA_WILDCARD_POSITION_HPP

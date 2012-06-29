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


#ifndef CPPA_IS_FORWARD_ITERATOR_HPP
#define CPPA_IS_FORWARD_ITERATOR_HPP

#include <type_traits>

#include "cppa/util/rm_ref.hpp"

namespace cppa { namespace util {

/**
 * @ingroup MetaProgramming
 * @brief Checks wheter @p T behaves like a forward iterator.
 */
template<typename T>
class is_forward_iterator {

    template<class C>
    static bool sfinae_fun (
        C* iter,
        // check for 'C::value_type C::operator*()' returning a non-void type
        typename rm_ref<decltype(*(*iter))>::type* = 0,
        // check for 'C& C::operator++()'
        typename std::enable_if<std::is_same<C&, decltype(++(*iter))>::value>::type* = 0,
        // check for 'bool C::operator==()'
        typename std::enable_if<std::is_same<bool, decltype(*iter == *iter)>::value>::type* = 0,
        // check for 'bool C::operator!=()'
        typename std::enable_if<std::is_same<bool, decltype(*iter != *iter)>::value>::type* = 0
    ) {
        return true;
    }

    static void sfinae_fun(void*) { }

    typedef decltype(sfinae_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // CPPA_IS_FORWARD_ITERATOR_HPP

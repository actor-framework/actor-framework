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


#ifndef CPPA_FROM_STRING_HPP
#define CPPA_FROM_STRING_HPP

#include <string>
#include <typeinfo>
#include <exception>

#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa {

/**
 * @brief Converts a string created by {@link cppa::to_string to_string}
 *        to its original value.
 * @param what String representation of a serialized value.
 * @returns An {@link cppa::object object} instance that contains
 *          the deserialized value.
 */
object from_string(const std::string& what);

/**
 * @brief Convenience function that deserializes a value from @p what and
 *        converts the result to @p T.
 * @throws std::logic_error if the result is not of type @p T.
 * @returns The deserialized value as instance of @p T.
 */
template<typename T>
T from_string(const std::string& what) {
    object o = from_string(what);
    const std::type_info& tinfo = typeid(T);
    if (tinfo == *(o.type())) {
        return std::move(get_ref<T>(o));
    }
    else {
        std::string error_msg = "expected type name ";
        error_msg += uniform_typeid(tinfo)->name();
        error_msg += " found ";
        error_msg += o.type()->name();
        throw std::logic_error(error_msg);
    }
}

} // namespace cppa

#endif // CPPA_FROM_STRING_HPP

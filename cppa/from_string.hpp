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


#ifndef FROM_STRING_HPP
#define FROM_STRING_HPP

#include <string>
#include <typeinfo>
#include <exception>

#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa {

object from_string(const std::string& what);

template<typename T>
T from_string(const std::string &what)
{
    object o = from_string(what);
    const std::type_info& tinfo = typeid(T);
    if (tinfo == *(o.type()))
    {
        return std::move(get<T>(o));
    }
    else
    {
        std::string error_msg = "expected type name ";
        error_msg += uniform_typeid(tinfo)->name();
        error_msg += " found ";
        error_msg += o.type()->name();
        throw std::logic_error(error_msg);
    }
}

} // namespace cppa

#endif // FROM_STRING_HPP

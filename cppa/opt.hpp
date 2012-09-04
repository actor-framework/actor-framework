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


#ifndef CPPA_OPT_HPP
#define CPPA_OPT_HPP

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <functional>

#include "cppa/on.hpp"
#include "cppa/option.hpp"
#include "cppa/detail/demangle.hpp"
#include "cppa/detail/opt_impls.hpp"

namespace cppa {

template<typename T>
typename detail::conv_arg_impl<T>::result_type conv_arg(const std::string& arg) {
    return detail::conv_arg_impl<T>::_(arg);
}

template<typename T>
detail::rd_arg_functor<T> rd_arg(T& storage) {
    return {storage};
}

template<typename T>
detail::add_arg_functor<T> add_arg(std::vector<T>& storage) {
    return {storage};
}

typedef std::map<std::string, std::map<std::pair<char, std::string>, std::string> >
        options_description;

detail::opt_rvalue_builder<true> on_opt(char short_opt,
                                        std::string long_opt,
                                        options_description* desc = nullptr,
                                        std::string help_text = "",
                                        std::string help_group = "general options");

decltype(on<std::string>()
        .when(cppa::placeholders::_x1.in(std::vector<std::string>())))
on_vopt(char short_opt,
        std::string long_opt,
        options_description* desc = nullptr,
        std::string help_text = "",
        std::string help_group = "general options");

std::function<void()> print_desc(options_description* desc,
                                 std::ostream& out = std::cout);

std::function<void()> print_desc_and_exit(options_description* desc,
                                          std::ostream& out = std::cout,
                                          int exit_reason = 0);

} // namespace cppa

#endif // CPPA_OPT_HPP

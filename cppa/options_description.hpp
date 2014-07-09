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


#ifndef CPPA_OPTIONS_DESCRIPTION_HPP
#define CPPA_OPTIONS_DESCRIPTION_HPP

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <functional>

#include "cppa/on.hpp"
#include "cppa/optional.hpp"
#include "cppa/detail/demangle.hpp"
#include "cppa/detail/opt_impls.hpp"

namespace cppa {

/**
 * @brief Right-hand side of a match expression for a program option
 *        reading an argument of type @p T.
 */
template<typename T>
detail::rd_arg_functor<T> rd_arg(T& storage) {
    return {storage};
}

/**
 * @brief Right-hand side of a match expression for a program option
 *        adding an argument of type @p T to @p storage.
 */
template<typename T>
detail::add_arg_functor<T> add_arg(std::vector<T>& storage) {
    return {storage};
}

inline std::function<void()> set_flag(bool& storage) {
    return [&] { storage = true; };
}

/**
 * @brief Stores a help text along with the number of expected arguments.
 */
struct option_info {
    std::string help_text;
    size_t num_args;
};

/**
 * @brief Stores a help text for program options with option groups.
 */
using options_description = std::map<
                                std::string,
                                std::map<
                                    std::pair<char, std::string>,
                                    option_info
                                >
                            >;

using opt_rvalue_builder = decltype(   on(std::function<optional<std::string>
                                          (const std::string&)>{})
                                    || on(std::string{}, val<std::string>));

using opt0_rvalue_builder = decltype(on(std::string{}) || on(std::string{}));

/**
 * @brief Left-hand side of a match expression for a program option with
 *        one argument.
 */
opt_rvalue_builder on_opt1(char short_opt,
                           std::string long_opt,
                           options_description* desc = nullptr,
                           std::string help_text = "",
                           std::string help_group = "general options");

/**
 * @brief Left-hand side of a match expression for a program option with
 *        no argument.
 */
opt0_rvalue_builder on_opt0(char short_opt,
                            std::string long_opt,
                            options_description* desc = nullptr,
                            std::string help_text = "",
                            std::string help_group = "general options");

/**
 * @brief Returns a function that prints the help text of @p desc to @p out.
 */
std::function<void()> print_desc(options_description* desc,
                                 std::ostream& out = std::cout);

/**
 * @brief Returns a function that prints the help text of @p desc to @p out
 *        and then calls <tt>exit(exit_reason)</tt>.
 */
std::function<void()> print_desc_and_exit(options_description* desc,
                                          std::ostream& out = std::cout,
                                          int exit_reason = 0);

} // namespace cppa

#endif // CPPA_OPTIONS_DESCRIPTION_HPP

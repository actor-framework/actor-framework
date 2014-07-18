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

#ifndef CPPA_OPT_HPP
#define CPPA_OPT_HPP

// <backward_compatibility version="0.9" whole_file="yes">
#include <map>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <functional>

#include "caf/on.hpp"
#include "caf/optional.hpp"

#include "caf/detail/demangle.hpp"

#include "cppa/opt_impls.hpp"

namespace caf {

using string_proj = std::function<optional<std::string> (const std::string&)>;

string_proj extract_longopt_arg(const std::string& prefix) {
    return [prefix](const std::string& arg) -> optional<std::string> {
        if (arg.compare(0, prefix.size(), prefix)) {
            return std::string(arg.begin() + prefix.size(), arg.end());
        }
        return none;
    };
}

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
inline opt_rvalue_builder on_opt1(char short_opt,
                                  std::string long_opt,
                                  options_description* desc = nullptr,
                                  std::string help_text = "",
                                  std::string help_group = "general options") {
    if (desc) {
        option_info oinf{move(help_text), 1};
        (*desc)[help_group].insert(std::make_pair(
                                     std::make_pair(short_opt, long_opt),
                                     std::move(oinf)));
    }
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    const char* lhs_str = short_flag_arr;
    std::string prefix = "--";
    prefix += long_opt;
    prefix += "=";
    std::function<optional<std::string> (const std::string&)> kvp =
        [prefix](const std::string& input) -> optional<std::string> {
            if (std::equal(std::begin(prefix), std::end(prefix), std::begin(input))) {
                return input.substr(prefix.size());
            }
            else if (std::equal(std::begin(prefix) + 1, std::end(prefix), std::begin(input))) {
                return input.substr(prefix.size() - 1);
            }
            return none;
        };
    std::vector<std::string> opts;
    opts.push_back(lhs_str);
    opts.push_back("--" + long_opt);
    opts.push_back("-" + long_opt);
    return on(extract_longopt_arg(prefix)) || on(lhs_str, val<std::string>);
}

/**
 * @brief Left-hand side of a match expression for a program option with
 *        no argument.
 */
inline opt0_rvalue_builder on_opt0(char short_opt,
                                   std::string long_opt,
                                   options_description* desc = nullptr,
                                   std::string help_text = "",
                                   std::string help_group = "general options") {
    if (desc) {
        option_info oinf{help_text, 0};
        (*desc)[help_group].insert(std::make_pair(
                                     std::make_pair(short_opt, long_opt),
                                     std::move(oinf)));
    }
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    std::string short_opt_string = short_flag_arr;
    return on(long_opt) || on(short_opt_string);
}

/**
 * @brief Returns a function that prints the help text of @p desc to @p out.
 */
std::function<void()> print_desc(options_description* desc,
                                 std::ostream& out = std::cout) {
    return [&out, desc] {
        if (!desc) return;
        if (desc->empty()) {
            out << "please use '-h' or '--help' for a list "
                   "of available program options\n";
        }
        for (auto& opt_group : *desc) {
            out << opt_group.first << ":\n";
            for (auto& opt : opt_group.second) {
                out << "  ";
                out << std::setw(40) << std::left;
                std::ostringstream tmp;
                auto& names = opt.first;
                if (names.first != '\0') {
                    tmp << "-" << names.first;
                    for (size_t num = 1; num <= opt.second.num_args; ++num) {
                        tmp << " <arg" << num << ">";
                    }
                    tmp << " | ";
                }
                tmp << "--" << names.second;
                if (opt.second.num_args > 0) {
                    tmp << "=<arg1>";
                }
                for (size_t num = 2; num <= opt.second.num_args; ++num) {
                    tmp << ", <arg" << num << ">";
                }
                out << tmp.str() << opt.second.help_text << "\n";
            }
            out << "\n";
        }
    };
}

/**
 * @brief Returns a function that prints the help text of @p desc to @p out
 *        and then calls <tt>exit(exit_reason)</tt>.
 */
inline std::function<void()> print_desc_and_exit(options_description* desc,
                                                 std::ostream& out = std::cout,
                                                 int exit_reason = 0) {
    auto fun = print_desc(desc, out);
    return [=] {
        fun();
        exit(exit_reason);
    };
}

} // namespace caf
// </backward_compatibility>

#endif // CPPA_OPT_HPP

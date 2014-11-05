/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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

inline string_proj extract_longopt_arg(const std::string& prefix) {
  return [prefix](const std::string& arg) -> optional<std::string> {
    if (arg.compare(0, prefix.size(), prefix) == 0) {
      return std::string(arg.begin() + prefix.size(), arg.end());
    }
    return none;
  };
}

/**
 * Right-hand side of a match expression for a program option
 * reading an argument of type `T`.
 */
template <class T>
detail::rd_arg_functor<T> rd_arg(T& storage) {
  return {storage};
}

/**
 * Right-hand side of a match expression for a program option
 * adding an argument of type `T` to `storage`.
 */
template <class T>
detail::add_arg_functor<T> add_arg(std::vector<T>& storage) {
  return {storage};
}

/**
 * Right-hand side of a match expression for a program option
 * reading a boolean flag.
 */
inline std::function<void()> set_flag(bool& storage) {
  return [&] { storage = true; };
}

/**
 * Stores a help text along with the number of expected arguments.
 */
struct option_info {
  std::string help_text;
  size_t num_args;
};

/**
 * Stores a help text for program options with option groups.
 */
using options_description =
  std::map<
    std::string,
    std::map<
      std::pair<char, std::string>,
      option_info
    >
  >;

using opt_rvalue_builder =
  decltype(on(std::function<optional<std::string> (const std::string&)>{})
           || on(std::string{}, val<std::string>));

using opt0_rvalue_builder = decltype(on(std::string{}) || on(std::string{}));

/**
 * Left-hand side of a match expression for a program option with one argument.
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
  prefix += std::move(long_opt);
  prefix += "=";
  return on(extract_longopt_arg(prefix)) || on(lhs_str, val<std::string>);
}

/**
 * Left-hand side of a match expression for a program option with no argument.
 */
inline opt0_rvalue_builder on_opt0(char short_opt,
                                   std::string long_opt,
                                   options_description* desc = nullptr,
                                   std::string help_text = "",
                                   std::string help_group = "general options") {
  if (desc) {
    option_info oinf{help_text, 0};
    (*desc)[help_group].insert(std::make_pair(std::make_pair(short_opt,
                                                             long_opt),
                                              std::move(oinf)));
  }
  const char short_flag_arr[] = {'-', short_opt, '\0' };
  std::string short_opt_string = short_flag_arr;
  return on("--" + long_opt) || on(short_opt_string);
}

/**
 * Returns a function that prints the help text of `desc` to `out`.
 */
inline std::function<void()> print_desc(options_description* desc,
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
 * Returns a function that prints the help text of `desc` to `out`
 *    and then calls `exit(exit_reason).
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

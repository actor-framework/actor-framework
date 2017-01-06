/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DETAIL_PARSE_INI_HPP
#define CAF_DETAIL_PARSE_INI_HPP

#include <string>
#include <istream>
#include <functional>

#include "caf/atom.hpp"
#include "caf/variant.hpp"
#include "caf/optional.hpp"
#include "caf/config_value.hpp"

namespace caf {
namespace detail {

struct parse_ini_t {
  /// Denotes an optional error output stream
  using opt_err = optional<std::ostream&>;
  /// Denotes a callback for consuming configuration values.
  using config_consumer = std::function<void (size_t, std::string,
                                              config_value&, opt_err)>;

  /// Parse the given input stream as INI formatted data and
  /// calls the consumer with every key-value pair.
  /// @param input Input stream of INI formatted text.
  /// @param consumer_fun Callback consuming generated key-value pairs.
  /// @param errors Output stream for parser errors.
  void operator()(std::istream& input,
                  const config_consumer& consumer_fun,
                  opt_err errors = none) const;

};

constexpr parse_ini_t parse_ini = parse_ini_t{};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PARSE_INI_HPP

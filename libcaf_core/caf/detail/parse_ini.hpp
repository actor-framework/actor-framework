/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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
  /// Denotes a callback for consuming configuration values.
  using config_consumer = std::function<void (size_t, std::string, config_value&)>;

  /// Parse the given input stream as INI formatted data and
  /// calls the consumer with every key-value pair.
  /// @param raw_data Input stream of INI formatted text.
  /// @param errors Output stream for parser errors.
  /// @param consumer Callback consuming generated key-value pairs.
  void operator()(std::istream& raw_data,
                  config_consumer consumer,
                  optional<std::ostream&> errors = none) const;

};

constexpr parse_ini_t parse_ini = parse_ini_t{};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PARSE_INI_HPP

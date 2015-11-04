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

#ifndef CAF_PARSER_CONFIG_HPP
#define CAF_PARSER_CONFIG_HPP

#include <string>
#include <istream>
#include <algorithm>

#include "caf/variant.hpp"
#include "caf/maybe.hpp"

namespace caf {

/// Denotes the format of a configuration file.
enum class config_format {
  ini
};

/// Denotes a configuration value.
using config_value = variant<std::string, double, int64_t, bool>;

/// Denotes a callback for config parser implementations.
using config_consumer = std::function<void (std::string, config_value)>;

/// Read configuration from `input_stream` using given `format`.
/// @param input_stream ASCII-formatted configuration.
/// @param format Configuration format such as INI.
/// @param errors Output streams for error messages.
void parse_config(std::istream& input_stream, config_format format,
                  maybe<std::ostream&> errors = none);

/// Read configuration from `file_name` using given `format` or try to
/// deduce file format automatically if `cf == none`.
/// @param file_name Path to configuration file.
/// @param cf Forces the parser to use a specific file format unless `none`.
/// @param errors Output streams for error messages.
void parse_config(const std::string& file_name,
                  maybe<config_format> cf = none,
                  maybe<std::ostream&> errors = none);

} // namespace caf

#endif // CAF_PARSER_CONFIG_HPP

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

#include <algorithm>
#include <string>

#include "caf/variant.hpp"

namespace caf {

enum class config_format {
  auto_detect,
  ini
};

using config_value = variant<std::string, double, int64_t, bool>;

using config_consumer = std::function<void (std::string, config_value)>;

/// parse_config
/// @param file_name
/// @param cf
void parse_config(const std::string& file_name,
                  config_format cf = config_format::auto_detect);

} // namespace caf

#endif // CAF_PARSER_CONFIG_HPP

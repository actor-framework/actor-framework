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

#include <istream>

#include "caf/parse_config.hpp"

namespace caf {

namespace detail {

/// Parse the given input stream as INI formatted data and calls the consumer
/// with every key-value pair.
/// @param raw_data the INI formatted input stream
/// @param errors a stream of all errors which occure while parsing
/// @param consumer a function that consums the key-value pairs
void parse_ini(std::istream& raw_data, std::ostream& errors,
               config_consumer consumer);

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PARSE_INI_HPP

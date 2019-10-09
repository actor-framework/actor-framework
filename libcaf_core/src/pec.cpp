/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/pec.hpp"

#include "caf/config_value.hpp"
#include "caf/error.hpp"
#include "caf/make_message.hpp"
#include "caf/string_view.hpp"

namespace caf {

error make_error(pec code) {
  return {static_cast<uint8_t>(code), atom("parser")};
}

error make_error(pec code, size_t line, size_t column) {
  config_value::dictionary context;
  context["line"] = line;
  context["column"] = column;
  return {static_cast<uint8_t>(code), atom("parser"),
          make_message(std::move(context))};
}

error make_error(pec code, string_view argument) {
  config_value::dictionary context;
  context["argument"] = std::string{argument.begin(), argument.end()};
  return {static_cast<uint8_t>(code), atom("parser"),
          make_message(std::move(context))};
}

} // namespace caf

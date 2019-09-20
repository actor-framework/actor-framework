/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/basp/ec.hpp"

#include "caf/atom.hpp"
#include "caf/error.hpp"
#include "caf/string_view.hpp"

namespace caf {
namespace net {
namespace basp {

namespace {

string_view ec_names[] = {
  "none",
  "invalid_magic_number",
  "unexpected_number_of_bytes",
  "unexpected_payload",
  "missing_payload",
  "illegal_state",
  "invalid_handshake",
  "missing_handshake",
  "unexpected_handshake",
  "version_mismatch",
  "unimplemented = 10",
  "app_identifiers_mismatch",
  "invalid_payload",
};

} // namespace

std::string to_string(ec x) {
  auto result = ec_names[static_cast<uint8_t>(x)];
  return std::string{result.begin(), result.end()};
}

error make_error(ec x) {
  return {static_cast<uint8_t>(x), atom("basp")};
}

} // namespace basp
} // namespace net
} // namespace caf

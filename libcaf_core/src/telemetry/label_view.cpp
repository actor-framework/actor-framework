/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/telemetry/label_view.hpp"

#include "caf/telemetry/label.hpp"

namespace caf::telemetry {

int label_view::compare(const label_view& x) const noexcept {
  auto cmp1 = name().compare(x.name());
  return cmp1 != 0 ? cmp1 : value().compare(x.value());
}

int label_view::compare(const label& x) const noexcept {
  auto cmp1 = name().compare(x.name());
  return cmp1 != 0 ? cmp1 : value().compare(x.value());
}

std::string to_string(const label_view& x) {
  std::string result;
  result.reserve(x.name().size() + 1 + x.value().size());
  result.insert(result.end(), x.name().begin(), x.name().end());
  result += '=';
  result.insert(result.end(), x.value().begin(), x.value().end());
  return result;
}

} // namespace caf::telemetry

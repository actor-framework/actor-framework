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

#pragma once

#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

namespace caf::detail {

template <class T>
void assign_inspector_try_result(error& x, T&& y) {
  x = std::forward<T>(y);
}

inline void assign_inspector_try_result(error_code<sec>& x, const error& y) {
  if (y.category() == type_id_v<sec>)
    x = static_cast<sec>(y.code());
  else
    x = sec::runtime_error;
}

inline void assign_inspector_try_result(error_code<sec>& x, error_code<sec> y) {
  x = y;
}

} // namespace caf::detail

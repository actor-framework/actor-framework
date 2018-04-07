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

#pragma once

#include <type_traits>

#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Constructs an object of type `T` in an `intrusive_ptr`.
/// @relates ref_counted
/// @relates intrusive_ptr
template <class T, class... Ts>
intrusive_ptr<T> make_counted(Ts&&... xs) {
  return intrusive_ptr<T>(new T(std::forward<Ts>(xs)...), false);
}

} // namespace caf


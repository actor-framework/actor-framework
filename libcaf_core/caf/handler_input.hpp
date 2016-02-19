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

#ifndef CAF_HANDLER_INPUT_HPP
#define CAF_HANDLER_INPUT_HPP

#include <type_traits>

#include "caf/atom.hpp"

namespace caf {

/// Defines `type` as `const T&` unless `T` is an arithmetic type. In the
/// latter case, `type` is an alias for `T`.
template <class T, bool IsArithmetic = std::is_arithmetic<T>::value>
struct handler_input {
  using type = const T&;
};

template <class T>
struct handler_input<T, true> {
  using type = T;
};

template <atom_value X>
struct handler_input<atom_constant<X>, false> {
  using type = atom_constant<X>;
};

template <class T>
using handler_input_t = typename handler_input<T>::type;

} // namespace caf

#endif // CAF_HANDLER_INPUT_HPP

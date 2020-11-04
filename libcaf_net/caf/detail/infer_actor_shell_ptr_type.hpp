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

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

namespace caf::detail {

template <class T>
struct actor_shell_ptr_type_oracle;

template <>
struct actor_shell_ptr_type_oracle<actor> {
  using type = net::actor_shell_ptr;
};

template <class... Sigs>
struct actor_shell_ptr_type_oracle<typed_actor<Sigs...>> {
  using type = net::typed_actor_shell_ptr<Sigs...>;
};

template <class T>
using infer_actor_shell_ptr_type =
  typename actor_shell_ptr_type_oracle<T>::type;

} // namespace caf::detail

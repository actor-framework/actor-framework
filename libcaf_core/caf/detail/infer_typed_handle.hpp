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

#ifndef CAF_DETAIL_INFER_TYPED_HANDLE_HPP
#define CAF_DETAIL_INFER_TYPED_HANDLE_HPP

#include "caf/typed_actor.hpp"
#include "caf/typed_behavior.hpp"

namespace caf {
namespace detail {

template <class...>
struct infer_typed_handle;

template <class... Rs, class... Ts>
struct infer_typed_handle<typed_behavior<Rs...>, Ts...> {
  using handle_type = typed_actor<Rs...>;
  using functor_base_type = functor_based_typed_actor<Rs...>;
};

template <class... Rs, class... Ts>
struct infer_typed_handle<void, typed_event_based_actor<Rs...>*, Ts...> {
  using handle_type = typed_actor<Rs...>;
  using functor_base_type = functor_based_typed_actor<Rs...>;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INFER_TYPED_HANDLE_HPP

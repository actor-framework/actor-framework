/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_TYPED_REMOTE_ACTOR_HELPER_HPP
#define CAF_IO_TYPED_REMOTE_ACTOR_HELPER_HPP

#include "caf/actor_cast.hpp"
#include "caf/typed_actor.hpp"

#include "caf/typed_actor.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/io/remote_actor_impl.hpp"

namespace caf {
namespace io {

template <class List>
struct typed_remote_actor_helper;

template <template <class...> class List, class... Ts>
struct typed_remote_actor_helper<List<Ts...>> {
  using return_type = typed_actor<Ts...>;
  template <class... Vs>
  return_type operator()(Vs&&... vs) {
    auto iface = return_type::message_types();
    auto tmp = remote_actor_impl(std::move(iface), std::forward<Vs>(vs)...);
    return actor_cast<return_type>(tmp);
  }
};

} // namespace io
} // namespace caf

#endif // CAF_IO_TYPED_REMOTE_ACTOR_HELPER_HPP

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_MAKE_ACTOR_HPP
#define CAF_MAKE_ACTOR_HPP

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/ref_counted.hpp"
#include "caf/infer_handle.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/actor_storage.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {

template <class T, class R = infer_handle_from_class_t<T>, class... Ts>
R make_actor(actor_id aid, node_id nid, actor_system* sys, Ts&&... xs) {
  CAF_LOG_SPAWN_EVENT(aid, std::forward_as_tuple(xs...));
  auto ptr = new actor_storage<T>(aid, std::move(nid), sys,
                                  std::forward<Ts>(xs)...);
  return {&(ptr->ctrl), false};
}

} // namespace caf

#endif // CAF_MAKE_ACTOR_HPP

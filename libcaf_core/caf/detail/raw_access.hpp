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

#ifndef CAF_DETAIL_RAW_ACCESS_HPP
#define CAF_DETAIL_RAW_ACCESS_HPP

#include "caf/actor.hpp"
#include "caf/group.hpp"
#include "caf/channel.hpp"
#include "caf/actor_addr.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/abstract_channel.hpp"

namespace caf {
namespace detail {

class raw_access {

public:

  template <class ActorHandle>
  static abstract_actor* get(const ActorHandle& hdl) {
    return hdl.ptr_.get();
  }

  static abstract_channel* get(const channel& hdl) {
    return hdl.ptr_.get();
  }

  static abstract_group* get(const group& hdl) {
    return hdl.ptr_.get();
  }

  static actor unsafe_cast(abstract_actor* ptr) {
    return {ptr};
  }

  static actor unsafe_cast(const actor_addr& hdl) {
    return {get(hdl)};
  }

  static actor unsafe_cast(const abstract_actor_ptr& ptr) {
    return {ptr.get()};
  }

  template <class T>
  static void unsafe_assign(T& lhs, const actor& rhs) {
    lhs = T{get(rhs)};
  }

  template <class T>
  static void unsafe_assign(T& lhs, const abstract_actor_ptr& ptr) {
    lhs = T{ptr.get()};
  }

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_RAW_ACCESS_HPP

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/abstract_channel.hpp"
#include "caf/abstract_group.hpp"
#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/channel.hpp"
#include "caf/group.hpp"

namespace caf::detail {

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

} // namespace caf::detail

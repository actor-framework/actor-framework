// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>
#include <utility>

#include "caf/behavior_policy.hpp"
#include "caf/fwd.hpp"

namespace caf::mixin {

/// A `behavior_changer` is an actor that supports
/// `self->become(...)` and `self->unbecome()`.
template <class Base, class Subtype>
class behavior_changer : public Base {
public:
  // -- member types -----------------------------------------------------------

  using extended_base = behavior_changer;

  using behavior_type = typename behavior_type_of<Subtype>::type;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  behavior_changer(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- behavior management ----------------------------------------------------

  void become(behavior_type bhvr) {
    dptr()->do_become(std::move(bhvr.unbox()), true);
  }

  void become(const keep_behavior_t&, behavior_type bhvr) {
    dptr()->do_become(std::move(bhvr.unbox()), false);
  }

  template <class T0, class T1, class... Ts>
  typename std::enable_if<
    !std::is_same<keep_behavior_t, typename std::decay<T0>::type>::value>::type
  become(T0&& x0, T1&& x1, Ts&&... xs) {
    behavior_type bhvr{std::forward<T0>(x0), std::forward<T1>(x1),
                       std::forward<Ts>(xs)...};
    dptr()->do_become(std::move(bhvr.unbox()), true);
  }

  template <class T0, class T1, class... Ts>
  void become(const keep_behavior_t&, T0&& x0, T1&& x1, Ts&&... xs) {
    behavior_type bhvr{std::forward<T0>(x0), std::forward<T1>(x1),
                       std::forward<Ts>(xs)...};
    dptr()->do_become(std::move(bhvr.unbox()), false);
  }

  void unbecome() {
    dptr()->bhvr_stack_.pop_back();
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }
};

} // namespace caf::mixin

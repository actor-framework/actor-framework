// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/abstract_broker.hpp"
#include "caf/io/datagram_servant.hpp"
#include "caf/io/doorman.hpp"
#include "caf/io/scribe.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/stateful_actor.hpp"

#include <map>
#include <vector>

namespace caf::io {

/// Describes a dynamically typed broker.
/// @extends abstract_broker
/// @ingroup Broker
class CAF_IO_EXPORT broker
  : public extend<abstract_broker, broker>::with<mixin::sender,
                                                 mixin::requester>,
    public dynamically_typed_actor_base {
public:
  using super = extended_base;

  using signatures = none_t;

  using behavior_type = behavior;

  template <class F, class... Ts>
  infer_handle_from_fun_t<F> fork(F fun, connection_handle hdl, Ts&&... xs) {
    CAF_ASSERT(context() != nullptr);
    auto sptr = this->take(hdl);
    CAF_ASSERT(sptr->hdl() == hdl);
    using trait = infer_handle_from_fun_trait_t<F>;
    using impl = typename trait::impl;
    actor_config cfg{context()};
    detail::init_fun_factory<impl, F> fac;
    cfg.init_fun = fac(std::move(fun), hdl, std::forward<Ts>(xs)...);
    auto res = this->system().spawn_class<impl, no_spawn_options>(cfg);
    auto forked = static_cast<impl*>(actor_cast<abstract_actor*>(res));
    forked->move_scribe(std::move(sptr));
    return res;
  }

  void initialize() override;

  using super::super;

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  auto mail(Args&&... args) {
    return event_based_mail(dynamically_typed{}, this,
                            std::forward<Args>(args)...);
  }

  // -- behavior management ----------------------------------------------------

  /// @copydoc event_based_actor::become
  template <class T, class... Ts>
  void become(T&& arg, Ts&&... args) {
    if constexpr (std::is_same_v<keep_behavior_t, std::decay_t<T>>) {
      static_assert(sizeof...(Ts) > 0);
      do_become(behavior{std::forward<Ts>(args)...}, false);
    } else {
      do_become(behavior{std::forward<T>(arg), std::forward<Ts>(args)...},
                true);
    }
  }

  /// @copydoc event_based_actor::unbecome
  void unbecome() {
    bhvr_stack_.pop_back();
  }

protected:
  virtual behavior make_behavior();
};

/// Convenience template alias for declaring state-based brokers.
template <class State>
using stateful_broker = stateful_actor<State, broker>;

} // namespace caf::io

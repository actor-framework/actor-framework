// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/abstract_broker.hpp"
#include "caf/io/datagram_servant.hpp"
#include "caf/io/doorman.hpp"
#include "caf/io/scribe.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/stateful_actor.hpp"

#include <map>
#include <vector>

namespace caf {

template <>
class behavior_type_of<io::broker> {
public:
  using type = behavior;
};

namespace io {

/// Describes a dynamically typed broker.
/// @extends abstract_broker
/// @ingroup Broker
class CAF_IO_EXPORT broker
  // clang-format off
  : public extend<abstract_broker, broker>::
           with<mixin::sender, mixin::requester,
                mixin::behavior_changer>,
    public dynamically_typed_actor_base {
  // clang-format on
public:
  using super
    = extend<abstract_broker, broker>::with<mixin::sender, mixin::requester,
                                            mixin::behavior_changer>;

  using signatures = none_t;

  template <class F, class... Ts>
  infer_handle_from_fun_t<F> fork(F fun, connection_handle hdl, Ts&&... xs) {
    CAF_ASSERT(context() != nullptr);
    auto sptr = this->take(hdl);
    CAF_ASSERT(sptr->hdl() == hdl);
    using impl = typename infer_handle_from_fun<F>::impl;
    actor_config cfg{context()};
    detail::init_fun_factory<impl, F> fac;
    cfg.init_fun = fac(std::move(fun), hdl, std::forward<Ts>(xs)...);
    auto res = this->system().spawn_class<impl, no_spawn_options>(cfg);
    auto forked = static_cast<impl*>(actor_cast<abstract_actor*>(res));
    forked->move_scribe(std::move(sptr));
    return res;
  }

  void initialize() override;

  explicit broker(actor_config& cfg);

  broker(broker&&) = delete;

  broker(const broker&) = delete;

  broker& operator=(broker&&) = delete;

  broker& operator=(const broker&) = delete;

protected:
  virtual behavior make_behavior();
};

/// Convenience template alias for declaring state-based brokers.
template <class State>
using stateful_broker = stateful_actor<State, broker>;

} // namespace io
} // namespace caf

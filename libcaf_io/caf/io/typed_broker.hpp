// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/abstract_broker.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/middleman.hpp"

#include "caf/actor_registry.hpp"
#include "caf/config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/extend.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/none.hpp"
#include "caf/typed_actor.hpp"

#include <map>
#include <type_traits>
#include <vector>

namespace caf::io {

/// Denotes a minimal "client" broker managing one or more connection
/// handles by reacting to `new_data_msg` and `connection_closed_msg`.
/// @relates typed_broker
using connection_handler = typed_actor<result<void>(new_data_msg),
                                       result<void>(connection_closed_msg)>;

/// Denotes a minimal "server" broker managing one or more accept
/// handles by reacting to `new_connection_msg` and `acceptor_closed_msg`.
/// The accept handler usually calls `self->fork(...)` when receiving
/// a `new_connection_msg`.
/// @relates typed_broker
using accept_handler = typed_actor<result<void>(new_connection_msg),
                                   result<void>(acceptor_closed_msg)>;

/// A typed broker mediates between actor systems and other
/// components in the network.
/// @extends local_actor
template <class... Sigs>
class typed_broker
  // clang-format off
  : public extend<abstract_broker, typed_broker<Sigs...>>::template
           with<mixin::sender,
                mixin::requester>,
    public statically_typed_actor_base {
  // clang-format on
public:
  using signatures = type_list<Sigs...>;

  using actor_hdl = typed_actor<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  using super =
    typename extend<abstract_broker, typed_broker<Sigs...>>::template with<
      mixin::sender, mixin::requester>;

  /// @cond PRIVATE

  std::set<std::string> message_types() const override {
    type_list<typed_actor<Sigs...>> hdl;
    return this->system().message_types(hdl);
  }

  /// @endcond

  void initialize() override {
    CAF_LOG_TRACE("");
    this->init_broker();
    auto bhvr = make_behavior();
    if (!bhvr) {
      log::io::debug("make_behavior() did not return a behavior: alive = {}",
                     this->alive());
    }
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      log::io::debug("make_behavior() did return a valid behavior");
      this->do_become(std::move(bhvr.unbox()), true);
    }
  }

  template <class F, class... Ts>
  infer_handle_from_fun_t<F> fork(F fun, connection_handle hdl, Ts&&... xs) {
    CAF_ASSERT(this->context() != nullptr);
    auto sptr = this->take(hdl);
    CAF_ASSERT(sptr->hdl() == hdl);
    using impl = typename infer_handle_from_fun_trait_t<F>::impl;
    static_assert(
      std::is_convertible<typename impl::actor_hdl, connection_handler>::value,
      "Cannot fork: new broker misses required handlers");
    actor_config cfg{this->context()};
    detail::init_fun_factory<impl, F> fac;
    cfg.init_fun = fac(std::move(fun), hdl, std::forward<Ts>(xs)...);
    using impl = infer_impl_from_fun_t<F>;
    static constexpr bool spawnable
      = detail::spawnable<F, impl, decltype(hdl), Ts...>();
    static_assert(spawnable,
                  "cannot spawn function-based broker with given arguments");
    auto res = this->system().spawn_functor(std::bool_constant<spawnable>{},
                                            cfg, fun, hdl,
                                            std::forward<Ts>(xs)...);
    auto forked = static_cast<impl*>(actor_cast<abstract_actor*>(res));
    forked->move_scribe(std::move(sptr));
    return res;
  }

  expected<connection_handle> add_tcp_scribe(const std::string& host,
                                             uint16_t port) {
    static_assert(std::is_convertible_v<actor_hdl, connection_handler>,
                  "Cannot add scribe: broker misses required handlers");
    return super::add_tcp_scribe(host, port);
  }

  connection_handle add_tcp_scribe(network::native_socket fd) {
    static_assert(std::is_convertible_v<actor_hdl, connection_handler>,
                  "Cannot add scribe: broker misses required handlers");
    return super::add_tcp_scribe(fd);
  }

  expected<std::pair<accept_handle, uint16_t>>
  add_tcp_doorman(uint16_t port = 0, const char* in = nullptr,
                  bool reuse_addr = false) {
    static_assert(std::is_convertible_v<actor_hdl, accept_handler>,
                  "Cannot add doorman: broker misses required handlers");
    return super::add_tcp_doorman(port, in, reuse_addr);
  }

  expected<accept_handle> add_tcp_doorman(network::native_socket fd) {
    static_assert(std::is_convertible_v<actor_hdl, accept_handler>,
                  "Cannot add doorman: broker misses required handlers");
    return super::add_tcp_doorman(fd);
  }

  explicit typed_broker(actor_config& cfg) : super(cfg) {
    // nop
  }

  /// @copydoc event_based_actor::become
  template <class T, class... Ts>
  void become(T&& arg, Ts&&... args) {
    if constexpr (std::is_same_v<keep_behavior_t, std::decay_t<T>>) {
      static_assert(sizeof...(Ts) > 0);
      this->do_become(behavior_type{std::forward<Ts>(args)...}.unbox(), false);
    } else {
      behavior_type bhv{std::forward<T>(arg), std::forward<Ts>(args)...};
      this->do_become(std::move(bhv).unbox(), true);
    }
  }

  /// @copydoc event_based_actor::unbecome
  void unbecome() {
    this->bhvr_stack_.pop_back();
  }

protected:
  virtual behavior_type make_behavior() {
    if (this->initial_behavior_fac_) {
      auto bhvr = this->initial_behavior_fac_(this);
      this->initial_behavior_fac_ = nullptr;
      if (bhvr)
        this->do_become(std::move(bhvr), true);
    }
    return behavior_type::make_empty_behavior();
  }
};

} // namespace caf::io

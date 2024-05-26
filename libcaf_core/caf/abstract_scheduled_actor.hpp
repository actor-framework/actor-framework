// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/behavior.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"
#include "caf/make_counted.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

template <class...>
struct response_to_flow_cell_helper;

} // namespace caf::detail

namespace caf {

/// A cooperatively scheduled, event-based actor implementation.
class CAF_CORE_EXPORT abstract_scheduled_actor
  : public local_actor,
    public non_blocking_actor_base {
public:
  // -- friends ----------------------------------------------------------------

  template <class...>
  friend class event_based_response_handle;

  template <class...>
  friend struct detail::response_to_flow_cell_helper;

  // -- member types -----------------------------------------------------------

  using super = local_actor;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  ~abstract_scheduled_actor() override;

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  virtual void add_awaited_response_handler(message_id response_id,
                                            behavior bhvr,
                                            disposable pending_timeout = {})
    = 0;

  /// Adds a callback for a multiplexed response.
  virtual void add_multiplexed_response_handler(message_id response_id,
                                                behavior bhvr,
                                                disposable pending_timeout = {})
    = 0;

  /// Calls the default error handler.
  virtual void call_error_handler(error& what) = 0;

  /// Runs all pending actions.
  virtual void run_actions() = 0;

  /// Lifts a response message into a flow cell in order to allow the actor to
  /// turn a response into an `observable` or `single`.
  template <class... Ts>
  auto response_to_flow_cell(message_id response_id,
                             disposable pending_timeout = {}) {
    detail::response_to_flow_cell_helper<Ts...> fn;
    return fn(this, response_id, std::move(pending_timeout));
  }

private:
  virtual flow::coordinator* flow_context() = 0;
};

} // namespace caf

namespace caf::detail {

template <>
struct response_to_flow_cell_helper<> {
  auto operator()(abstract_scheduled_actor* self, message_id response_id,
                  disposable pending_timeout) const {
    auto cell = make_counted<flow::op::cell<unit_t>>(self->flow_context());
    auto bhvr = behavior{
      [self, cell] {
        cell->set_value(unit);
        self->run_actions();
      },
      [self, cell](error& err) {
        cell->set_error(std::move(err));
        self->run_actions();
      },
    };
    self->add_multiplexed_response_handler(response_id, std::move(bhvr),
                                           std::move(pending_timeout));
    return cell;
  }
};

template <class T>
struct response_to_flow_cell_helper<T> {
  auto operator()(abstract_scheduled_actor* self, message_id response_id,
                  disposable pending_timeout) const {
    auto cell = make_counted<flow::op::cell<T>>(self->flow_context());
    auto bhvr = behavior{
      [self, cell](T& value) {
        cell->set_value(std::move(value));
        self->run_actions();
      },
      [self, cell](error& err) {
        cell->set_error(std::move(err));
        self->run_actions();
      },
    };
    self->add_multiplexed_response_handler(response_id, std::move(bhvr),
                                           std::move(pending_timeout));
    return cell;
  }
};

template <class T1, class T2, class... Ts>
struct response_to_flow_cell_helper<T1, T2, Ts...> {
  auto operator()(abstract_scheduled_actor* self, message_id response_id,
                  disposable pending_timeout) const {
    using tuple_t = cow_tuple<T1, T2, Ts...>;
    auto cell = make_counted<flow::op::cell<tuple_t>>(self->flow_context());
    auto bhvr = behavior{
      [self, cell](T1& val1, T2& val2, Ts&... vals) {
        cell->set_value(
          make_cow_tuple(std::move(val1), std::move(val2), std::move(vals)...));
        self->run_actions();
      },
      [self, cell](error& err) {
        cell->set_error(std::move(err));
        self->run_actions();
      },
    };
    self->add_multiplexed_response_handler(response_id, std::move(bhvr),
                                           std::move(pending_timeout));
    return cell;
  }
};

} // namespace caf::detail

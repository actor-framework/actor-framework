// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async_mail.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"

#include <functional>
#include <memory>
#include <shared_mutex>

namespace caf {

/// An co-existing actor forwarding all messages through a user-defined
/// callback to another object, thus serving as gateway to
/// allow any object to interact with other actors.
/// @extends local_actor
class CAF_CORE_EXPORT actor_companion
  : public extend<scheduled_actor, actor_companion>::with<mixin::sender> {
public:
  // -- member types -----------------------------------------------------------

  using super = extended_base;

  /// Required by `spawn` for type deduction.
  using signatures = none_t;

  /// Required by `spawn` for type deduction.
  using behavior_type = behavior;

  /// Delegates incoming messages to user-defined event loop.
  using enqueue_handler = std::function<void(mailbox_element_ptr)>;

  /// Callback for actor termination.
  using on_exit_handler = std::function<void()>;

  // -- constructors, destructors ----------------------------------------------

  using super::super;

  ~actor_companion() override;

  // -- overridden functions ---------------------------------------------------

  bool enqueue(mailbox_element_ptr ptr, execution_unit* host) override;

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  void on_exit() override;

  // -- modifiers --------------------------------------------------------------

  /// Removes the handler for incoming messages and terminates
  /// the companion for exit reason `rsn`.
  void disconnect(exit_reason rsn = exit_reason::normal);

  /// Sets the handler for incoming messages.
  /// @warning `handler` needs to be thread-safe
  void on_enqueue(enqueue_handler handler);

  /// Sets the handler for incoming messages.
  void on_exit(on_exit_handler handler);

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  [[nodiscard]] auto mail(Args&&... args) {
    return async_mail(dynamically_typed{}, this, std::forward<Args>(args)...);
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

private:
  // set by parent to define custom enqueue action
  enqueue_handler on_enqueue_;

  // custom code for on_exit()
  on_exit_handler on_exit_;

  // guards access to handler_
  std::shared_mutex lock_;
};

} // namespace caf

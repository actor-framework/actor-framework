// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/mailer.hpp"
#include "caf/policy/sender.hpp"
#include "caf/scheduled_actor.hpp"

#include <functional>
#include <memory>
#include <shared_mutex>

namespace caf {

/// An co-existing actor forwarding all messages through a user-defined
/// callback to another object, thus serving as gateway to
/// allow any object to interact with other actors.
/// @extends local_actor
class CAF_CORE_EXPORT actor_companion : public abstract_actor {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_actor;

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

  bool enqueue(mailbox_element_ptr ptr, scheduler* sched) override;

  const char* name() const override;

  // -- modifiers --------------------------------------------------------------

  /// Sets the handler for incoming messages.
  /// @warning `handler` needs to be thread-safe
  /// @note If the mailbox is already closed, the call has no effect.
  void on_enqueue(enqueue_handler handler);

  /// Sets the callback invoked when the actor terminates (see @ref on_cleanup).
  /// @note If the mailbox is already closed, the call has no effect.
  void on_exit(on_exit_handler handler);

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  [[nodiscard]] auto mail(Args&&... args) {
    using mailer_policy = policy::sender;
    return make_mailer<mailer_policy>(this, std::forward<Args>(args)...);
  }

  // -- miscellaneous ----------------------------------------------------------

  void setup_metrics() {
    // nop; required by `make_actor`
  }

protected:
  void on_cleanup(const error& reason) override;

private:
  bool try_force_close_mailbox() override;

  /// Guards access to other member variables of the actor companion.
  mutable std::shared_mutex companion_mtx_;

  /// Tracks whether the mailbox has been closed.
  bool closed_ = false;

  /// User-defined handler for incoming messages.
  enqueue_handler on_enqueue_;

  /// User-defined callback for actor termination.
  on_exit_handler on_exit_;
};

} // namespace caf

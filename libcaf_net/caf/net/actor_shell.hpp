/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/local_actor.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/net/fwd.hpp"
#include "caf/none.hpp"
#include "caf/policy/normal_messages.hpp"

namespace caf::net {

///
class actor_shell
  : public extend<local_actor, actor_shell>::with<mixin::sender>,
    public dynamically_typed_actor_base,
    public non_blocking_actor_base {
public:
  // -- friends ----------------------------------------------------------------

  friend class actor_shell_ptr;

  // -- member types -----------------------------------------------------------

  using super = extend<local_actor, actor_shell>::with<mixin::sender>;

  using signatures = none_t;

  using behavior_type = behavior;

  struct mailbox_policy {
    using queue_type = intrusive::drr_queue<policy::normal_messages>;

    using deficit_type = policy::normal_messages::deficit_type;

    using mapped_type = policy::normal_messages::mapped_type;

    using unique_pointer = policy::normal_messages::unique_pointer;
  };

  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit actor_shell(actor_config& cfg, socket_manager* owner);

  ~actor_shell() override;

  // -- state modifiers --------------------------------------------------------

  /// Detaches the shell from its owner and closes the mailbox.
  void quit(error reason);

  // -- mailbox access ---------------------------------------------------------

  auto& mailbox() noexcept {
    return mailbox_;
  }

  /// Dequeues and returns the next message from the mailbox or returns
  /// `nullptr` if the mailbox is empty.
  mailbox_element_ptr next_message();

  /// Dequeues and processes the next message from the mailbox.
  /// @param bhvr Available message handlers.
  /// @param fallback Callback for processing message that failed to match
  ///                 `bhvr`.
  /// @returns `true` if a message was dequeued and process, `false` if the
  ///          mailbox was empty.
  bool consume_message(behavior& bhvr,
                       callback<result<message>(message&)>& fallback);

  /// Tries to put the mailbox into the `blocked` state, causing the next
  /// enqueue to register the owning socket manager for write events.
  bool try_block_mailbox();

  // -- overridden functions of abstract_actor ---------------------------------

  using abstract_actor::enqueue;

  void enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

  mailbox_element* peek_at_next_mailbox_element() override;

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  bool cleanup(error&& fail_state, execution_unit* host) override;

private:
  // Stores incoming actor messages.
  mailbox_type mailbox_;

  // Guards access to owner_.
  std::mutex owner_mtx_;

  // Points to the owning manager (nullptr after quit was called).
  socket_manager* owner_;
};

/// An "owning" pointer to an actor shell in the sense that it calls `quit()` on
/// the shell when going out of scope.
class actor_shell_ptr {
public:
  friend class socket_manager;

  constexpr actor_shell_ptr() noexcept {
    // nop
  }

  constexpr actor_shell_ptr(std::nullptr_t) noexcept {
    // nop
  }

  actor_shell_ptr(actor_shell_ptr&& other) noexcept = default;

  actor_shell_ptr& operator=(actor_shell_ptr&& other) noexcept = default;

  actor_shell_ptr(const actor_shell_ptr& other) = delete;

  actor_shell_ptr& operator=(const actor_shell_ptr& other) = delete;

  ~actor_shell_ptr();

  /// Returns an actor handle to the managed actor shell.
  actor as_actor() const noexcept;

  void detach(error reason);

  actor_shell* get() const noexcept;

  actor_shell* operator->() const noexcept {
    return get();
  }

  actor_shell& operator*() const noexcept {
    return *get();
  }

  bool operator!() const noexcept {
    return !ptr_;
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

private:
  /// @pre `ptr != nullptr`
  explicit actor_shell_ptr(strong_actor_ptr ptr) noexcept;

  strong_actor_ptr ptr_;
};

} // namespace caf::net

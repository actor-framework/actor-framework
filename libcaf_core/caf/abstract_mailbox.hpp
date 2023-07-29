// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/inbox_result.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

/// The base class for all mailbox implementations.
class CAF_CORE_EXPORT abstract_mailbox {
public:
  virtual ~abstract_mailbox();

  /// Adds a new element to the mailbox.
  /// @returns `inbox_result::success` if the element has been added to the
  ///          mailbox, `inbox_result::unblocked_reader` if the reader has been
  ///          unblocked, or `inbox_result::queue_closed` if the mailbox has
  ///          been closed.
  /// @threadsafe
  virtual intrusive::inbox_result push_back(mailbox_element_ptr ptr) = 0;

  /// Adds a new element to the mailbox by putting it in front of the queue.
  /// @note Only the owning actor is allowed to call this function.
  virtual void push_front(mailbox_element_ptr ptr) = 0;

  /// Removes the next element from the mailbox.
  /// @returns The next element in the mailbox or `nullptr` if the mailbox is
  ///          empty.
  /// @note Only the owning actor is allowed to call this function.
  virtual mailbox_element_ptr pop_front() = 0;

  /// Checks whether the mailbox has been closed.
  /// @note Only the owning actor is allowed to call this function.
  virtual bool closed() const noexcept = 0;

  /// Checks whether the owner of this mailbox is currently waiting for new
  /// messages.
  /// @note Only the owning actor is allowed to call this function.
  virtual bool blocked() const noexcept = 0;

  /// Tries to put the mailbox in a blocked state.
  /// @note Only the owning actor is allowed to call this function.
  virtual bool try_block() = 0;

  /// Tries to put the mailbox in an empty state from a blocked state.
  /// @note Only the owning actor is allowed to call this function.
  virtual bool try_unblock() = 0;

  /// Closes the mailbox and discards all pending messages.
  /// @returns The number of dropped messages.
  /// @note Only the owning actor is allowed to call this function.
  virtual size_t close(const error& reason) = 0;

  /// Returns the number of pending messages.
  /// @note Only the owning actor is allowed to call this function.
  virtual size_t size() = 0;

  /// Increases the reference count by one.
  virtual void ref_mailbox() noexcept = 0;

  /// Decreases the reference count by one and deletes this instance if the
  /// reference count drops to zero.
  virtual void deref_mailbox() noexcept = 0;

  /// Checks whether the mailbox is empty.
  bool empty() {
    return size() == 0;
  }

  /// @private
  virtual mailbox_element* peek(message_id id) = 0;
};

} // namespace caf

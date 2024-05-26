// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/intrusive/inbox_result.hpp"

#include <atomic>
#include <memory>

namespace caf::intrusive {

/// An intrusive, thread-safe LIFO queue implementation for a single reader
/// with any number of writers.
template <class T>
class lifo_inbox {
public:
  // -- member types -----------------------------------------------------------

  using value_type = T;

  using pointer = value_type*;

  using node_type = typename value_type::node_type;

  using node_pointer = node_type*;

  using unique_pointer = std::unique_ptr<value_type>;

  using deleter_type = typename unique_pointer::deleter_type;

  // -- static utility functions -----------------------------------------------

  /// Casts a node type to its value type.
  static pointer promote(node_pointer ptr) noexcept {
    return static_cast<pointer>(ptr);
  }

  // -- modifiers --------------------------------------------------------------

  /// Tries to enqueue a new element to the inbox.
  /// @threadsafe
  inbox_result push_front(pointer new_element) noexcept {
    CAF_ASSERT(new_element != nullptr);
    pointer e = stack_.load();
    auto eof = stack_closed_tag();
    auto blk = reader_blocked_tag();
    while (e != eof) {
      // A tag is never part of a non-empty list.
      new_element->next = e != blk ? e : nullptr;
      if (stack_.compare_exchange_strong(e, new_element))
        return e == reader_blocked_tag() ? inbox_result::unblocked_reader
                                         : inbox_result::success;
      // Continue with new value of `e`.
    }
    // The queue has been closed, drop messages.
    deleter_type d;
    d(new_element);
    return inbox_result::queue_closed;
  }

  /// Tries to enqueue a new element to the inbox.
  /// @threadsafe
  inbox_result push_front(unique_pointer x) noexcept {
    return push_front(x.release());
  }

  /// Tries to enqueue a new element to the mailbox.
  /// @threadsafe
  template <class... Ts>
  inbox_result emplace_front(Ts&&... xs) {
    return push_front(new value_type(std::forward<Ts>(xs)...));
  }

  /// Queries whether this queue is empty.
  /// @pre `!closed() && !blocked()`
  bool empty() const noexcept {
    CAF_ASSERT(!closed());
    CAF_ASSERT(!blocked());
    return stack_.load() == stack_empty_tag();
  }

  /// Queries whether this has been closed.
  bool closed() const noexcept {
    return stack_.load() == stack_closed_tag();
  }

  /// Queries whether this has been marked as blocked, i.e.,
  /// the owner of the list is waiting for new data.
  bool blocked() const noexcept {
    return stack_.load() == reader_blocked_tag();
  }

  /// Tries to set this queue from `empty` to `blocked`.
  bool try_block() noexcept {
    auto e = stack_empty_tag();
    return stack_.compare_exchange_strong(e, reader_blocked_tag());
  }

  /// Tries to set this queue from `blocked` to `empty`.
  bool try_unblock() noexcept {
    auto e = reader_blocked_tag();
    return stack_.compare_exchange_strong(e, stack_empty_tag());
  }

  /// Sets the head to `new_head` and returns the previous head if the queue
  /// was not empty.
  pointer take_head(pointer new_head) noexcept {
    // This member function should only be used to transition to closed or
    // empty.
    CAF_ASSERT(new_head == stack_closed_tag() || new_head == stack_empty_tag());
    pointer e = stack_.load();
    // Must not be called on a closed queue.
    CAF_ASSERT(e != stack_closed_tag());
    // Must not be called on a blocked queue unless for setting it to closed,
    // because that would mean an actor accesses its mailbox after blocking its
    // mailbox but before receiving anything.
    CAF_ASSERT(e != reader_blocked_tag() || new_head == stack_closed_tag());
    // We don't assert these conditions again since only the owner is allowed
    // to call this member function, i.e., there's never a race on `take_head`.
    while (e != new_head) {
      if (stack_.compare_exchange_weak(e, new_head)) {
        CAF_ASSERT(e != stack_closed_tag());
        if (is_empty_or_blocked_tag(e)) {
          // Sanity check: going from empty/blocked to closed.
          CAF_ASSERT(new_head == stack_closed_tag());
          return nullptr;
        }
        return e;
      }
      // Next iteration.
    }
    return nullptr;
  }

  /// Sets the head to `stack_empty_tag()` and returns the previous head if
  /// the queue was not empty.
  pointer take_head() noexcept {
    return take_head(stack_empty_tag());
  }

  /// Closes this queue and deletes all remaining elements.
  /// @warning Call only from the reader (owner).
  void close() {
    close(deleter_type{});
  }

  /// Closes this queue and applies `f` to each pointer. The function object
  /// `f` must take ownership of the passed pointer.
  /// @warning Call only from the reader (owner).
  template <class F>
  void close(F&& f) {
    node_pointer ptr = take_head(stack_closed_tag());
    while (ptr != nullptr) {
      auto next = ptr->next;
      f(promote(ptr));
      ptr = next;
    }
  }

  lifo_inbox() noexcept {
    stack_ = stack_empty_tag();
  }

  ~lifo_inbox() noexcept {
    if (!closed())
      close();
  }

private:
  static constexpr pointer stack_empty_tag() {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator whether this queue is empty or not.
    return static_cast<pointer>(nullptr);
  }

  pointer stack_closed_tag() const noexcept {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator whether this queue is closed or not.
    return reinterpret_cast<pointer>(reinterpret_cast<intptr_t>(this) + 1);
  }

  pointer reader_blocked_tag() const noexcept {
    // We are *never* going to dereference the returned pointer. It is only
    // used as indicator whether the owner of the queue is currently waiting for
    // new messages.
    return reinterpret_cast<pointer>(const_cast<lifo_inbox*>(this));
  }

  bool is_empty_or_blocked_tag(pointer x) const noexcept {
    return x == stack_empty_tag() || x == reader_blocked_tag();
  }

  // -- member variables ------------------------------------------------------

  std::atomic<pointer> stack_;
};

} // namespace caf::intrusive

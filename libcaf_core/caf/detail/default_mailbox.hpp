// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_mailbox.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive/lifo_inbox.hpp"
#include "caf/intrusive/linked_list.hpp"

#include <atomic>
#include <cstddef>

namespace caf::detail {

/// Our default mailbox implementation. Uses a LIFO inbox for storing incoming
/// messages and combines it with two FIFO caches for storing urgent and normal
/// messages.
class CAF_CORE_EXPORT default_mailbox : public abstract_mailbox {
public:
  default_mailbox() noexcept : ref_count_(1) {
    // nop
  }

  default_mailbox(const default_mailbox&) = delete;

  default_mailbox& operator=(const default_mailbox&) = delete;

  mailbox_element* peek(message_id id) override;

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override;

  void push_front(mailbox_element_ptr ptr) override;

  mailbox_element_ptr pop_front() override;

  bool closed() const noexcept override;

  bool blocked() const noexcept override;

  bool try_block() override;

  bool try_unblock() override;

  size_t close(const error&) override;

  size_t size() override;

  void ref_mailbox() noexcept override;

  void deref_mailbox() noexcept override;

  size_t ref_count() const noexcept {
    return ref_count_.load();
  }

private:
  /// Returns the total number of elements stored in the queues.
  size_t cached() const noexcept {
    return urgent_queue_.size() + normal_queue_.size();
  }

  /// Tries to fetch more messages from the LIFO inbox.
  bool fetch_more();

  /// Stores urgent messages in FIFO order.
  intrusive::linked_list<mailbox_element> urgent_queue_;

  /// Stores normal messages in FIFO order.
  intrusive::linked_list<mailbox_element> normal_queue_;

  /// Stores incoming messages in LIFO order.
  alignas(CAF_CACHE_LINE_SIZE) intrusive::lifo_inbox<mailbox_element> inbox_;

  /// The intrusive reference count.
  alignas(CAF_CACHE_LINE_SIZE) std::atomic<size_t> ref_count_;
};

} // namespace caf::detail

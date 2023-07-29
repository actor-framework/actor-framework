// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_mailbox.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/task_queue.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/urgent_messages.hpp"

namespace caf::detail {

class default_mailbox : public abstract_mailbox {
public:
  struct policy {
    using mapped_type = mailbox_element;

    using task_size_type = size_t;

    using deficit_type = size_t;

    using unique_pointer = mailbox_element_ptr;

    static size_t task_size(const mapped_type&) noexcept {
      return 1;
    }
  };

  default_mailbox() = default;

  default_mailbox(const default_mailbox&) = delete;

  default_mailbox& operator=(const default_mailbox&) = delete;

  mailbox_element* peek(message_id id);

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override;

  void push_front(mailbox_element_ptr ptr) override;

  mailbox_element_ptr pop_front() override;

  bool closed() const noexcept override;

  bool blocked() const noexcept override;

  bool try_block() override;

  bool try_unblock() override;

  size_t close(const error&) override;

  size_t size() override;

private:
  /// Returns the total number of elements stored in the queues.
  size_t cached() const noexcept {
    return urgent_queue_.total_task_size() + normal_queue_.total_task_size();
  }

  /// Tries to fetch more messages from the LIFO inbox.
  bool fetch_more();

  /// Stores incoming messages in LIFO order.
  alignas(CAF_CACHE_LINE_SIZE) intrusive::lifo_inbox<policy> inbox_;

  /// Stores urgent messages in FIFO order.
  intrusive::task_queue<policy> urgent_queue_;

  /// Stores normal messages in FIFO order.
  intrusive::task_queue<policy> normal_queue_;
};

} // namespace caf::detail

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/resumable.hpp"

namespace caf {

/// Implements a simple proxy forwarding all operations to a manager.
class forwarding_actor_proxy : public actor_proxy, public resumable {
public:
  // -- member types -----------------------------------------------------------

  /// Stores asynchronous messages with default priority.
  using normal_queue = intrusive::drr_queue<policy::normal_messages>;

  /// Stores asynchronous messages with hifh priority.
  using urgent_queue = intrusive::drr_queue<policy::urgent_messages>;


  /// Configures the FIFO inbox with two nested queues:
  ///
  ///   1. Default asynchronous messages
  ///   2. High-priority asynchronous messages
  struct mailbox_policy {
    using deficit_type = size_t;

    using mapped_type = mailbox_element;

    using unique_pointer = mailbox_element_ptr;

    using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
      policy::categorized, normal_queue, urgent_queue>;

    static constexpr size_t normal_queue_index = 0;

    static constexpr size_t urgent_queue_index = 1;
  };

  /// A queue optimized for single-reader-many-writers.
  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

  // -- constructors and destructors -------------------------------------------

  forwarding_actor_proxy(actor_config& cfg, actor dest);

  ~forwarding_actor_proxy() override;

  // -- overridden member functions --------------------------------------------

  void enqueue(mailbox_element_ptr what, execution_unit* context) override;

  bool add_backlink(abstract_actor* x) override;

  bool remove_backlink(abstract_actor* x) override;

  void kill_proxy(execution_unit* ctx, error rsn) override;

  resume_result resume(execution_unit*, size_t max_throughput) override;

  void intrusive_ptr_add_ref_impl() override;

  void intrusive_ptr_release_impl() override;

private:
  // -- member variables -------------------------------------------------------

  // used by both event-based and blocking actors
  mailbox_type mailbox_;

  mutable detail::shared_spinlock broker_mtx_;
  actor broker_;
};

} // namespace caf


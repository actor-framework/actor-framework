/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/resumable.hpp"

namespace caf {
namespace io {
namespace basp {

/// Serializes any message it receives and forwards it to the BASP broker.
class proxy : public actor_proxy, public resumable {
public:
  // -- member types -----------------------------------------------------------

  using super = actor_proxy;

  /// Required by `spawn`, `anon_send`, etc. for type deduction.
  using signatures = none_t;

  /// Stores asynchronous messages with default priority.
  using normal_queue = intrusive::drr_queue<policy::normal_messages>;

  /// Stores asynchronous messages with hifh priority.
  using urgent_queue = intrusive::drr_queue<policy::urgent_messages>;

  /// Stores upstream messages.
  using upstream_queue = intrusive::drr_queue<policy::upstream_messages>;

  /// Stores downstream messages.
  using downstream_queue = intrusive::drr_queue<
    policy::downstream_messages::nested>;

  /// Configures the FIFO inbox with four nested queues:
  ///
  ///   1. Default asynchronous messages
  ///   2. High-priority asynchronous messages
  ///   3. Upstream messages
  ///   4. Downstream messages
  ///
  /// The queue for downstream messages is in turn composed of a nested queues,
  /// one for each active input slot.
  struct mailbox_policy {
    using deficit_type = size_t;

    using mapped_type = mailbox_element;

    using unique_pointer = mailbox_element_ptr;

    using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
      policy::categorized, urgent_queue, normal_queue, upstream_queue,
      downstream_queue>;
  };

  /// A queue optimized for single-reader-many-writers.
  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

  // -- constructors and destructors -------------------------------------------

  proxy(actor_config& cfg, actor dispatcher);

  ~proxy() override;

  // -- properties -------------------------------------------------------------

  const actor& dispatcher() {
    return dispatcher_;
  }

  execution_unit* context() {
    return context_;
  }

  // -- implementation of abstract_actor ---------------------------------------

  void enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

  bool add_backlink(abstract_actor* x) override;

  bool remove_backlink(abstract_actor* x) override;

  mailbox_element* peek_at_next_mailbox_element() override;

  // -- implementation of monitorable_actor ------------------------------------

  void on_cleanup(const error& reason) override;

  // -- implementation of actor_proxy ------------------------------------------

  void kill_proxy(execution_unit* ctx, error reason) override;

  // -- implementation of resumable --------------------------------------------

  resume_result resume(execution_unit* ctx, size_t max_throughput) override;

  void intrusive_ptr_add_ref_impl() override;

  void intrusive_ptr_release_impl() override;

private:
  // -- member variables -------------------------------------------------------

  /// Stores incoming messages.
  mailbox_type mailbox_;

  /// Actor for dispatching serialized BASP messages.
  actor dispatcher_;

  /// Points to the current execution unit when being resumed.
  execution_unit* context_;
};

} // namespace basp
} // namespace io
} // namespace caf

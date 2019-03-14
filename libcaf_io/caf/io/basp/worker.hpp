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

#include <atomic>
#include <cstdint>
#include <vector>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/io/basp/fwd.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/basp/remote_message_handler.hpp"
#include "caf/node_id.hpp"
#include "caf/resumable.hpp"

namespace caf {
namespace io {
namespace basp {

/// Deserializes payloads for BASP messages asynchronously.
class worker : public resumable, public remote_message_handler<worker> {
public:
  // -- friends ----------------------------------------------------------------

  friend worker_hub;

  friend remote_message_handler<worker>;

  // -- member types -----------------------------------------------------------

  using atomic_pointer = std::atomic<worker*>;

  using scheduler_type = scheduler::abstract_coordinator;

  using buffer_type = std::vector<char>;

  // -- constructors, destructors, and assignment operators --------------------

  ~worker() override;

  // -- management -------------------------------------------------------------

  void launch(const node_id& last_hop, const basp::header& hdr,
              const buffer_type& payload);

  // -- implementation of resumable --------------------------------------------

  subtype_t subtype() const override;

  resume_result resume(execution_unit* ctx, size_t) override;

  void intrusive_ptr_add_ref_impl() override;

  void intrusive_ptr_release_impl() override;

private:
  // -- constructors, destructors, and assignment operators --------------------

  /// Only the ::worker_hub has access to the construtor.
  worker(worker_hub& hub, message_queue& queue, proxy_registry& proxies);

  // -- constants and assertions -----------------------------------------------

  /// Stores how many bytes the "first half" of this object requires.
  static constexpr size_t pointer_members_size = sizeof(atomic_pointer)
                                                 + sizeof(worker_hub*)
                                                 + sizeof(message_queue*)
                                                 + sizeof(proxy_registry*)
                                                 + sizeof(actor_system*);

  static_assert(CAF_CACHE_LINE_SIZE > pointer_members_size,
                "invalid cache line size");

  // -- member variables -------------------------------------------------------

  /// Points to the next worker in the hub.
  atomic_pointer next_;

  /// Points to our home hub.
  worker_hub* hub_;

  /// Points to the queue for establishing strict ordering.
  message_queue* queue_;

  /// Points to our proxy registry / factory.
  proxy_registry* proxies_;

  /// Points to the parent system.
  actor_system* system_;

  /// Prevents false sharing when writing to `next`.
  char pad_[CAF_CACHE_LINE_SIZE - pointer_members_size];

  /// ID for local ordering.
  uint64_t msg_id_;

  /// Identifies the node that sent us `hdr_` and `payload_`.
  node_id last_hop_;

  /// The header for the next message. Either a direct_message or a
  /// routed_message.
  header hdr_;

  /// Contains whatever this worker deserializes next.
  buffer_type payload_;
};

} // namespace basp
} // namespace io
} // namespace caf

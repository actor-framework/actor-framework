// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/basp/fwd.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/basp/remote_message_handler.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/config.hpp"
#include "caf/detail/abstract_worker.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/detail/worker_hub.hpp"
#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/resumable.hpp"

#include <cstdint>

namespace caf::io::basp {

/// Deserializes payloads for BASP messages asynchronously.
class CAF_IO_EXPORT worker : public detail::abstract_worker,
                             public remote_message_handler<worker> {
public:
  // -- friends ----------------------------------------------------------------

  friend remote_message_handler<worker>;

  // -- member types -----------------------------------------------------------

  using super = detail::abstract_worker;

  using hub_type = detail::worker_hub<worker>;

  // -- constructors, destructors, and assignment operators --------------------

  /// Only the ::worker_hub has access to the constructor.
  worker(hub_type& hub, message_queue& queue, proxy_registry& proxies);

  ~worker() override;

  // -- management -------------------------------------------------------------

  void launch(const node_id& last_hop, const basp::header& hdr,
              const byte_buffer& payload);

  // -- implementation of resumable --------------------------------------------

  resume_result resume(scheduler* ctx, size_t) override;

private:
  // -- constants and assertions -----------------------------------------------

  /// Stores how many bytes the "first half" of this object requires.
  static constexpr size_t pointer_members_size
    = sizeof(hub_type*) + sizeof(message_queue*) + sizeof(proxy_registry*)
      + sizeof(actor_system*);

  static_assert(CAF_CACHE_LINE_SIZE > pointer_members_size,
                "invalid cache line size");

  // -- member variables -------------------------------------------------------

  /// Points to our home hub.
  hub_type* hub_;

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
  byte_buffer payload_;
};

} // namespace caf::io::basp

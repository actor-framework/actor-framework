// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"

namespace caf::async {

/// Base type for asynchronous producers of events.
class CAF_CORE_EXPORT producer : public abstract_ref_counted {
public:
  virtual ~producer();

  /// Called to signal that the consumer started handling events.
  virtual void on_consumer_ready() = 0;

  /// Called to signal that the consumer stopped handling events.
  virtual void on_consumer_cancel() = 0;

  /// Called to signal that the consumer requests more events.
  /// @param demand The number of items that the producer may push to the buffer
  ///               without going past its capacity.
  /// @param unblocked `true` when a producer received `try_again_later` from
  ///                  `try_push` prior and may now resume pushing items to the
  ///                  buffer.
  virtual void on_consumer_demand(size_t demand, bool unblocked) = 0;
};

/// @relates producer
using producer_ptr = intrusive_ptr<producer>;

} // namespace caf::async

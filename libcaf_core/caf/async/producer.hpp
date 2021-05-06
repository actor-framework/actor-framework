// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"

namespace caf::async {

/// Base type for asynchronous producers of events.
class CAF_CORE_EXPORT producer {
public:
  virtual ~producer();

  /// Called to signal that the consumer started handling events.
  virtual void on_consumer_ready() = 0;

  /// Called to signal that the consumer stopped handling events.
  virtual void on_consumer_cancel() = 0;

  /// Called to signal that the consumer requests more events.
  virtual void on_consumer_demand(size_t demand) = 0;

  /// Increases the reference count of the producer.
  virtual void ref_producer() const noexcept = 0;

  /// Decreases the reference count of the producer and destroys the object if
  /// necessary.
  virtual void deref_producer() const noexcept = 0;
};

/// @relates producer
inline void intrusive_ptr_add_ref(const producer* p) noexcept {
  p->ref_producer();
}

/// @relates producer
inline void intrusive_ptr_release(const producer* p) noexcept {
  p->deref_producer();
}

/// @relates producer
using producer_ptr = intrusive_ptr<producer>;

} // namespace caf::async

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/credit_controller.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::detail {

/// A credit controller that estimates the bytes required to store incoming
/// batches and constrains credit based on upper bounds for memory usage.
class CAF_CORE_EXPORT token_based_credit_controller : public credit_controller {
public:
  // -- constants --------------------------------------------------------------

  /// Configures how many samples we require for recalculating buffer sizes.
  static constexpr int32_t min_samples = 50;

  /// Stores how many elements we buffer at most after the handshake.
  int32_t initial_buffer_size = 10;

  /// Stores how many elements we allow per batch after the handshake.
  int32_t initial_batch_size = 2;

  // -- constructors, destructors, and assignment operators --------------------

  explicit token_based_credit_controller(local_actor* self);

  ~token_based_credit_controller() override;

  // -- interface functions ----------------------------------------------------

  void before_processing(downstream_msg::batch& batch) override;

  calibration init() override;

  calibration calibrate() override;

  // -- factory functions ------------------------------------------------------

  template <class T>
  static auto make(local_actor* self, stream<T>) {
    return std::make_unique<token_based_credit_controller>(self);
  }

private:
  // --  see caf::defaults::stream::token_policy -------------------------------

  int32_t batch_size_;

  int32_t buffer_size_;
};

} // namespace caf::detail

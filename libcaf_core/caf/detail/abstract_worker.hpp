// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/resumable.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT abstract_worker : public resumable {
public:
  // -- friends ----------------------------------------------------------------

  friend abstract_worker_hub;

  // -- constructors, destructors, and assignment operators --------------------

  abstract_worker();

  ~abstract_worker() override;

  // -- implementation of resumable --------------------------------------------

  void ref() const noexcept final;

  void deref() const noexcept final;

private:
  // -- member variables -------------------------------------------------------

  mutable detail::atomic_ref_count ref_count_;

  /// Points to the next worker in the hub.
  std::atomic<abstract_worker*> next_;
};

} // namespace caf::detail

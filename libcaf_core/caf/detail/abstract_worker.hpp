// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"
#include "caf/resumable.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT abstract_worker : public ref_counted, public resumable {
public:
  // -- friends ----------------------------------------------------------------

  friend abstract_worker_hub;

  // -- constructors, destructors, and assignment operators --------------------

  abstract_worker();

  ~abstract_worker() override;

  // -- implementation of resumable --------------------------------------------

  subtype_t subtype() const noexcept final;

  void ref_resumable() const noexcept final;

  void deref_resumable() const noexcept final;

private:
  // -- member variables -------------------------------------------------------

  /// Points to the next worker in the hub.
  std::atomic<abstract_worker*> next_;
};

} // namespace caf::detail

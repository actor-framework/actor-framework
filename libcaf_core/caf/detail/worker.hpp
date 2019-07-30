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

#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"
#include "caf/resumable.hpp"

namespace caf {
namespace detail {

class worker : public ref_counted, public resumable {
public:
  // -- friends ----------------------------------------------------------------

  friend worker_hub;

  // -- constructors, destructors, and assignment operators --------------------

  worker();

  ~worker() override;

  // -- implementation of resumable --------------------------------------------

  subtype_t subtype() const override;

  void intrusive_ptr_add_ref_impl() override;

  void intrusive_ptr_release_impl() override;

private:
  // -- member variables -------------------------------------------------------

  /// Points to the next worker in the hub.
  std::atomic<worker*> next_;

  /// Points to our home hub.
  worker_hub* hub_;

  /// Points to the parent system.
  actor_system* system_;
};

} // namespace detail
} // namespace caf

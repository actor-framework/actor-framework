/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/ref_counted.hpp"

namespace caf {

ref_counted::~ref_counted() {
  // nop
}

ref_counted::ref_counted() : rc_(1) {
  // nop
}

ref_counted::ref_counted(const ref_counted&) : rc_(1) {
  // nop; don't copy reference count
}

ref_counted& ref_counted::operator=(const ref_counted&) {
  // nop; intentionally don't copy reference count
  return *this;
}

void ref_counted::deref() noexcept {
  if (unique()) {
    request_deletion(false);
    return;
  }
  if (rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    request_deletion(true);
  }
}

} // namespace caf

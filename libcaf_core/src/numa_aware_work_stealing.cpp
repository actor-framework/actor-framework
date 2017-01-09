/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/policy/numa_aware_work_stealing.hpp"

namespace caf {
namespace policy {

numa_aware_work_stealing::~numa_aware_work_stealing() {
  // nop
}

std::ostream&
operator<<(std::ostream& s,
           const numa_aware_work_stealing::hwloc_bitmap_wrapper& w) {
  char* tmp = nullptr;
  hwloc_bitmap_asprintf(&tmp, w.get());
  s << std::string(tmp);
  free(tmp);
  return s;
}


} // namespace policy
} // namespace caf

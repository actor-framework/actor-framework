/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/invalid_stream_scatterer.hpp"

#include "caf/logger.hpp"

namespace caf {

invalid_stream_scatterer::invalid_stream_scatterer(local_actor* self)
    : stream_scatterer(self) {
  // nop
}

invalid_stream_scatterer::~invalid_stream_scatterer() {
  // nop
}

void invalid_stream_scatterer::emit_batches() {
  // nop
}

size_t invalid_stream_scatterer::capacity() const noexcept {
  return 0u;
}

size_t invalid_stream_scatterer::buffered() const noexcept {
  return 0u;
}

} // namespace caf

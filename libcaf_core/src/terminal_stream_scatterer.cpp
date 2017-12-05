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

#include "caf/terminal_stream_scatterer.hpp"

#include <limits>

namespace caf {

terminal_stream_scatterer::terminal_stream_scatterer(local_actor* self)
    : super(self) {
  // nop
}

terminal_stream_scatterer::~terminal_stream_scatterer() {
  // nop
}

size_t terminal_stream_scatterer::capacity() const noexcept {
  return std::numeric_limits<size_t>::max();
}

} // namespace caf

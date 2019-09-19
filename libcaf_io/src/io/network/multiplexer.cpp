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

#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/default_multiplexer.hpp" // default singleton

namespace caf {
namespace io {
namespace network {

multiplexer::multiplexer(actor_system* sys)
    : execution_unit(sys),
      tid_(std::this_thread::get_id()) {
  // nop
}

multiplexer_ptr multiplexer::make(actor_system& sys) {
  CAF_LOG_TRACE("");
  return multiplexer_ptr{new default_multiplexer(&sys)};
}

multiplexer_backend* multiplexer::pimpl() {
  return nullptr;
}

multiplexer::supervisor::~supervisor() {
  // nop
}

resumable::subtype_t multiplexer::runnable::subtype() const {
  return resumable::function_object;
}

void multiplexer::runnable::intrusive_ptr_add_ref_impl() {
  intrusive_ptr_add_ref(this);
}

void multiplexer::runnable::intrusive_ptr_release_impl() {
  intrusive_ptr_release(this);
}

} // namespace network
} // namespace io
} // namespace caf

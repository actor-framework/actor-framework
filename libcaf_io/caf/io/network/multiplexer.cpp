// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/multiplexer.hpp"

#include "caf/io/network/default_multiplexer.hpp" // default singleton

namespace caf::io::network {

multiplexer::multiplexer(actor_system* sys)
  : execution_unit(sys), tid_(std::this_thread::get_id()) {
  // nop
}

multiplexer_ptr multiplexer::make(actor_system& sys) {
  auto lg = log::io::trace("");
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

} // namespace caf::io::network

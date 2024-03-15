// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/multiplexer.hpp"

#include "caf/io/network/default_multiplexer.hpp" // default singleton

namespace caf::io::network {

multiplexer::multiplexer(actor_system& sys)
  : tid_(std::this_thread::get_id()), sys_(&sys) {
  // nop
}

multiplexer_ptr multiplexer::make(actor_system& sys) {
  auto lg = log::io::trace("");
  return multiplexer_ptr{new default_multiplexer(sys)};
}

multiplexer_backend* multiplexer::pimpl() {
  return nullptr;
}

multiplexer::supervisor::~supervisor() {
  // nop
}

resumable::subtype_t multiplexer::runnable::subtype() const noexcept {
  return resumable::function_object;
}

void multiplexer::runnable::ref_resumable() const noexcept {
  ref();
}

void multiplexer::runnable::deref_resumable() const noexcept {
  deref();
}

void multiplexer::start() {
  // nop
}

void multiplexer::stop() {
  // nop
}

} // namespace caf::io::network

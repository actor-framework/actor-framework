// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/multiplexer_base.hpp"

#include "caf/io/network/default_multiplexer.hpp" // default singleton
#include "caf/io/network/multiplexer_supervisor.hpp"

namespace caf::io::network {

multiplexer_base::multiplexer_base(actor_system& sys)
  : tid_(std::this_thread::get_id()), sys_(&sys) {
  // nop
}

std::unique_ptr<multiplexer_base> multiplexer_base::make(actor_system& sys) {
  auto lg = log::io::trace("");
  return std::unique_ptr<multiplexer_base>{new default_multiplexer(sys)};
}

multiplexer_backend* multiplexer_base::pimpl() {
  return nullptr;
}

void multiplexer_base::start() {
  // nop
}

void multiplexer_base::stop() {
  // nop
}

bool multiplexer_base::is_system_scheduler() const noexcept {
  return false;
}

multiplexer_supervisor::~multiplexer_supervisor() {
  // nop
}

} // namespace caf::io::network

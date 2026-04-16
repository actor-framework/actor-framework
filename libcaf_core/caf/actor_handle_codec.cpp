// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_handle_codec.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/deserializer.hpp"
#include "caf/serializer.hpp"

namespace caf {

actor_handle_codec::~actor_handle_codec() noexcept {
  // nop
}

bool actor_handle_codec::save(serializer& sink, const weak_actor_ptr& ptr) {
  auto tmp = ptr.lock();
  return save(sink, tmp);
}

bool actor_handle_codec::load(deserializer& source, weak_actor_ptr& ptr) {
  strong_actor_ptr tmp;
  if (!load(source, tmp))
    return false;
  ptr = std::move(tmp);
  return true;
}

} // namespace caf

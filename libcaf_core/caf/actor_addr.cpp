// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_addr.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/compare.hpp"
#include "caf/detail/print.hpp"
#include "caf/node_id.hpp"

namespace caf {

actor_addr::actor_addr(actor_control_block* ptr) noexcept {
  if (ptr != nullptr) {
    id_ = ptr->id();
    node_ = ptr->node();
  }
}
intptr_t actor_addr::compare(const actor_addr& other) const noexcept {
  return detail::compare(*this, other);
}

intptr_t actor_addr::compare(const strong_actor_ptr& other) const noexcept {
  return detail::compare(*this, other.get());
}

intptr_t actor_addr::compare(const weak_actor_ptr& other) const noexcept {
  return detail::compare(*this, other.ctrl());
}

intptr_t actor_addr::compare(const abstract_actor* other) const noexcept {
  if (other) {
    return detail::compare(*this, other->ctrl());
  }
  return detail::compare(*this, nullptr);
}

intptr_t actor_addr::compare(const actor_control_block* other) const noexcept {
  return detail::compare(*this, other);
}

void actor_addr::swap(actor_addr& other) noexcept {
  using std::swap;
  swap(id_, other.id_);
  swap(node_, other.node_);
}

std::string to_string(const actor_addr& x) {
  std::string result;
  append_to_string(result, x);
  return result;
}

void append_to_string(std::string& dst, const actor_addr& addr) {
  if (addr.id() == 0) {
    dst += "null";
    return;
  }
  using namespace std::literals;
  auto out = std::back_inserter(dst);
  const detail::simple_formatter<node_id> prefix;
  out = prefix.format(addr.node(), out);
  const auto infix = "/actor/id/"sv;
  detail::print_iterator_adapter buf{out};
  buf.insert(buf.end(), infix.begin(), infix.end());
  print(buf, addr.id());
}

} // namespace caf

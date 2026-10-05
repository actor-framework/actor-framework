// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_addr.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/print.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/node_id.hpp"

namespace caf {

namespace {

constinit const actor_addr null_addr = actor_addr{};

} // namespace

actor_addr::actor_addr(actor_control_block* ptr) noexcept {
  if (ptr != nullptr) {
    id_ = ptr->id();
    node_ = ptr->node();
  }
}

const actor_addr& actor_addr::from(const abstract_actor* ptr) noexcept {
  if (ptr) {
    return ptr->address();
  }
  return null_addr;
}

const actor_addr& actor_addr::from(const actor_control_block* ptr) noexcept {
  if (ptr) {
    return ptr->address();
  }
  return null_addr;
}

const actor_addr& actor_addr::from(const strong_actor_ptr& ptr) noexcept {
  if (ptr) {
    return ptr->address();
  }
  return null_addr;
}

const actor_addr& actor_addr::from(const weak_actor_ptr& ptr) noexcept {
  if (ptr) {
    return ptr.ctrl()->address();
  }
  return null_addr;
}

void actor_addr::swap(actor_addr& other) noexcept {
  using std::swap;
  swap(id_, other.id_);
  swap(node_, other.node_);
}

size_t actor_addr::hash() const noexcept {
  auto aid = id();
  if (aid == 0) {
    return 0;
  }
  return caf::hash::fnv<size_t>::compute(aid, node());
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

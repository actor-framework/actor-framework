#include "caf/detail/compare.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/node_id.hpp"

namespace caf::detail {

intptr_t compare(actor_id lid, const node_id& lnode, actor_id rid,
                 const node_id& rnode) noexcept {
  if (lid == rid) {
    return lnode.compare(rnode);
  }
  return lid < rid ? -1 : 1;
}

intptr_t compare(const actor_control_block* lhs,
                 const actor_control_block* rhs) noexcept {
  if (lhs == rhs) {
    return 0;
  }
  if (lhs) {
    if (rhs) {
      return compare(lhs->id(), lhs->node(), rhs->id(), rhs->node());
    }
    return compare(lhs->id(), lhs->node(), invalid_actor_id, node_id{});
  }
  if (rhs) {
    return compare(invalid_actor_id, node_id{}, rhs->id(), rhs->node());
  }
  return 0;
}

intptr_t compare(const actor_control_block* lhs,
                 const actor_addr& rhs) noexcept {
  if (lhs) {
    return compare(lhs->id(), lhs->node(), rhs.id(), rhs.node());
  }
  return compare(invalid_actor_id, node_id{}, rhs.id(), rhs.node());
}

intptr_t compare(const actor_addr& lhs,
                 const actor_control_block* rhs) noexcept {
  if (rhs) {
    return compare(lhs.id(), lhs.node(), rhs->id(), rhs->node());
  }
  return compare(lhs.id(), lhs.node(), invalid_actor_id, node_id{});
}

intptr_t compare(const actor& lhs, const actor_control_block* rhs) noexcept {
  return compare(actor_cast<actor_control_block*>(lhs), rhs);
}

intptr_t compare(const actor_control_block* lhs, const actor& rhs) noexcept {
  return compare(lhs, actor_cast<actor_control_block*>(rhs));
}

intptr_t compare(const actor_addr& lhs, const actor_addr& rhs) noexcept {
  return compare(lhs.id(), lhs.node(), rhs.id(), rhs.node());
}

} // namespace caf::detail

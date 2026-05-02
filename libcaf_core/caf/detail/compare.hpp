
// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::detail {

CAF_CORE_EXPORT intptr_t compare(actor_id lid, const node_id& lnode,
                                 actor_id rid, const node_id& rnode) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor_control_block* lhs,
                                 const actor_control_block* rhs) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor_addr& lhs,
                                 const actor_control_block* rhs) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor_control_block* lhs,
                                 const actor_addr& rhs) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor& lhs,
                                 const actor_control_block* rhs) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor_control_block* lhs,
                                 const actor& rhs) noexcept;

CAF_CORE_EXPORT intptr_t compare(const actor_addr& lhs,
                                 const actor_addr& rhs) noexcept;

} // namespace caf::detail

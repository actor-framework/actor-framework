// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <memory>
#include <type_traits>

namespace caf::detail {

class CAF_CORE_EXPORT actor_system_config_access {
public:
  explicit actor_system_config_access(actor_system_config& cfg) : cfg_(&cfg) {
    // nop
  }

  void mailbox_factory(std::unique_ptr<detail::mailbox_factory> factory);

  internal::core_config& core();

private:
  actor_system_config* cfg_;
};

class CAF_CORE_EXPORT const_actor_system_config_access {
public:
  explicit const_actor_system_config_access(const actor_system_config& cfg)
    : cfg_(&cfg) {
    // nop
  }

  const internal::core_config& core() const;

private:
  const actor_system_config* cfg_;
};

} // namespace caf::detail

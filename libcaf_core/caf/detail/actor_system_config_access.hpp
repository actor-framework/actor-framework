// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_factory.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <memory>
#include <span>
#include <string_view>

namespace caf::detail {

// Note: implemented in actor_system_config.cpp
class CAF_CORE_EXPORT actor_system_config_access {
public:
  using module_factory_fn = actor_system_module* (*) (actor_system&);

  explicit actor_system_config_access(actor_system_config& cfg) : cfg_(&cfg) {
    // nop
  }

  std::span<module_factory_fn> module_factories();

  caf::actor_factory* actor_factory(std::string_view name);

  std::span<std::unique_ptr<thread_hook>> thread_hooks();

  void mailbox_factory(std::unique_ptr<detail::mailbox_factory> factory);

  detail::mailbox_factory* mailbox_factory();

private:
  actor_system_config* cfg_;
};

} // namespace caf::detail

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_system_module.hpp"
#include "caf/detail/core_export.hpp"

#include <functional>

namespace caf::detail {

/// A module that starts and manages background threads.
class CAF_CORE_EXPORT daemons : public caf::actor_system_module {
public:
  daemons();

  daemons(const daemons&) = delete;

  daemons& operator=(const daemons&) = delete;

  ~daemons() override;

  /// Removes the state associated with a stopped background thread.
  void cleanup(int id);

  bool launch(std::function<void()> fn, std::function<void()> stop);

  // -- overrides --------------------------------------------------------------

  void start() override;

  void stop() override;

  void init(caf::actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

private:
  struct impl;
  impl* impl_;
};

} // namespace caf::detail

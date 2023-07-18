// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_channel.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"

#include <memory>
#include <string>

namespace caf {

/// Interface for user-defined multicast implementations.
class CAF_CORE_EXPORT group_module : public ref_counted {
public:
  // -- constructors, destructors, and assignment operators --------------------

  group_module(actor_system& sys, std::string mname);

  group_module(const group_module&) = delete;

  group_module& operator=(const group_module&) = delete;

  ~group_module() override;

  // -- pure virtual member functions ------------------------------------------

  /// Stops all groups from this module.
  virtual void stop() = 0;

  /// Returns a pointer to the group associated with the name `group_name`.
  /// @threadsafe
  virtual expected<group> get(const std::string& group_name) = 0;

  // -- observers --------------------------------------------------------------

  /// Returns the hosting actor system.
  actor_system& system() const noexcept {
    return *system_;
  }

  /// Returns the name of this module implementation.
  const std::string& name() const noexcept {
    return name_;
  }

private:
  actor_system* system_;
  std::string name_;
};

using group_module_ptr = intrusive_ptr<group_module>;

} // namespace caf

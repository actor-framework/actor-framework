// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <functional>
#include <unordered_map>

#include "caf/abstract_group.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/group_module.hpp"

namespace caf {

class CAF_CORE_EXPORT group_manager {
public:
  // -- friends ----------------------------------------------------------------

  friend class actor_system;
  friend class group;
  friend class io::middleman;

  // -- member types -----------------------------------------------------------

  using modules_map = std::unordered_map<std::string, group_module_ptr>;

  using get_remote_fun
    = std::function<expected<group>(const node_id& origin,
                                    const std::string& module_name,
                                    const std::string& group_identifier)>;

  // -- constructors, destructors, and assignment operators --------------------

  group_manager(const group_manager&) = delete;

  group_manager& operator=(const group_manager&) = delete;

  ~group_manager();

  // -- observers --------------------------------------------------------------

  /// Get a handle to the group associated with given URI scheme.
  /// @threadsafe
  /// @experimental
  expected<group> get(std::string group_uri);

  /// Get a handle to the group associated with
  /// `identifier` from the module `mod_name`.
  /// @threadsafe
  expected<group> get(const std::string& module_name,
                      const std::string& group_identifier);

  /// Get a pointer to the group associated with
  /// `identifier` from the module `local`.
  /// @threadsafe
  group get_local(const std::string& group_identifier);

  /// Returns an anonymous group.
  /// Each calls to this member function returns a new instance
  /// of an anonymous group. Anonymous groups can be used whenever
  /// a set of actors wants to communicate using an exclusive channel.
  group anonymous();

  /// Returns the module named `name` if it exists, otherwise `none`.
  group_module_ptr get_module(const std::string& x) const;

private:
  get_remote_fun get_remote;

  // -- constructors, destructors, and assignment operators --------------------

  explicit group_manager(actor_system& sys);

  // -- member functions required by actor_system ------------------------------

  void init(actor_system_config& cfg);

  void start();

  void stop();

  // -- data members -----------------------------------------------------------

  modules_map mmap_;
  actor_system* system_;
  std::atomic<size_t> ad_hoc_id_;
};

} // namespace caf

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

#include <type_traits>

namespace caf {

/// Denotes the component that launched a CAF thread.
enum class thread_owner {
  /// Indicates that the thread belongs to the cooperative scheduler.
  scheduler,
  /// Indicates that the thread belongs to the internal thread pool for detached
  /// and blocking actors.
  pool,
  /// Indicates that the thread runs background activity such as logging for the
  /// actor system.
  system,
  /// Indicates that the thread has been launched by request of a user without
  /// using any of the default mechanisms above.
  other,
};

/// @relates sec
CAF_CORE_EXPORT std::string to_string(thread_owner);

/// @relates thread_owner
CAF_CORE_EXPORT bool from_string(std::string_view, thread_owner&);

/// @relates thread_owner
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<thread_owner>,
                                  thread_owner&);

/// @relates sec
template <class Inspector>
bool inspect(Inspector& f, thread_owner& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf

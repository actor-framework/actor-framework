// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 29-49 (Error.tex)

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf {

// --(rst-exit-reason-begin)--
/// This error category represents fail conditions for actors.
enum class exit_reason : uint8_t {
  /// Indicates that an actor finished execution without error.
  normal = 0,
  /// Indicates that an actor died because of an unhandled exception.
  unhandled_exception [[deprecated("superseded by sec::runtime_error")]],
  /// Indicates that the exit reason for this actor is unknown, i.e.,
  /// the actor has been terminated and no longer exists.
  unknown,
  /// Indicates that an actor pool unexpectedly ran out of workers.
  out_of_workers,
  /// Indicates that an actor was forced to shutdown by a user-generated event.
  user_shutdown,
  /// Indicates that an actor was killed unconditionally.
  kill,
  /// Indicates that an actor finished execution because a connection
  /// to a remote link was closed unexpectedly.
  remote_link_unreachable,
  /// Indicates that an actor was killed because it became unreachable.
  unreachable
};
// --(rst-exit-reason-end)--

/// @relates exit_reason
CAF_CORE_EXPORT std::string to_string(exit_reason);

/// @relates exit_reason
CAF_CORE_EXPORT bool from_string(std::string_view, exit_reason&);

/// @relates exit_reason
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<exit_reason>,
                                  exit_reason&);

template <class Inspector>
bool inspect(Inspector& f, exit_reason& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf

CAF_ERROR_CODE_ENUM(exit_reason)

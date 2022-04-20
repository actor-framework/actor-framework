// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <memory>
#include <typeinfo>

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/execution_unit.hpp"
#include "caf/exit_reason.hpp"

namespace caf {

class abstract_actor;

/// Callback utility class.
class CAF_CORE_EXPORT attachable {
public:
  attachable() = default;
  attachable(const attachable&) = delete;
  attachable& operator=(const attachable&) = delete;

  /// Represents a pointer to a value with its subtype as type ID number.
  struct token {
    /// Identifies a non-matchable subtype.
    static constexpr size_t anonymous = 0;

    /// Identifies `abstract_group::subscription`.
    static constexpr size_t subscription = 1;

    /// Identifies `default_attachable::observe_token`.
    static constexpr size_t observer = 2;

    /// Identifies `stream_aborter::token`.
    static constexpr size_t stream_aborter = 3;

    template <class T>
    token(const T& tk) : subtype(T::token_type), ptr(&tk) {
      // nop
    }

    /// Denotes the type of ptr.
    size_t subtype;

    /// Any value, used to identify attachable instances.
    const void* ptr;

    token(size_t typenr, const void* vptr);
  };

  virtual ~attachable();

  /// Executed if the actor finished execution with given `reason`.
  /// The default implementation does nothing.
  /// @warning `host` can be `nullptr`
  virtual void actor_exited(const error& fail_state, execution_unit* host);

  /// Returns `true` if `what` selects this instance, otherwise `false`.
  virtual bool matches(const token& what);

  /// Returns `true` if `what` selects this instance, otherwise `false`.
  template <class T>
  bool matches(const T& what) {
    return matches(token{T::token_type, &what});
  }

  std::unique_ptr<attachable> next;
};

/// @relates attachable
using attachable_ptr = std::unique_ptr<attachable>;

} // namespace caf

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/exit_reason.hpp"
#include "caf/fwd.hpp"
#include "caf/message_priority.hpp"

#include <cstdint>
#include <memory>
#include <typeinfo>

namespace caf {

/// @relates attachable
using attachable_ptr = std::unique_ptr<attachable>;

/// Callback utility class.
class CAF_CORE_EXPORT attachable {
public:
  // -- member types -----------------------------------------------------------

  /// Represents a pointer to a value with its subtype as type ID number.
  struct token {
    /// Identifies a non-matchable subtype.
    static constexpr size_t anonymous = 0;

    /// Identifies `abstract_group::subscription`.
    static constexpr size_t subscription = 1;

    /// Identifies `default_attachable::observe_token`.
    static constexpr size_t observer = 2;

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

  // -- constructors and destructors -------------------------------------------

  attachable() = default;
  attachable(const attachable&) = delete;
  attachable& operator=(const attachable&) = delete;

  virtual ~attachable();

  // -- interface for the actor ------------------------------------------------

  /// Executed if the actor finished execution with given `reason`.
  /// The default implementation does nothing.
  /// @warning `host` can be `nullptr`
  virtual void actor_exited(const error& fail_state, scheduler* sched);

  /// Returns `true` if `what` selects this instance, otherwise `false`.
  virtual bool matches(const token& what);

  /// Returns `true` if `what` selects this instance, otherwise `false`.
  template <class T>
  bool matches(const T& what) {
    return matches(token{T::token_type, &what});
  }

  // -- member variables -------------------------------------------------------

  attachable_ptr next;
};

} // namespace caf

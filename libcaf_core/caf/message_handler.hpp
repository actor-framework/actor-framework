// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/detail/behavior_impl.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/ref_counted.hpp"
#include "caf/timeout_definition.hpp"

#include <list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace caf {

/// A partial function implementation used to process a `message`.
class CAF_CORE_EXPORT message_handler {
public:
  friend class behavior;

  message_handler() = default;
  message_handler(message_handler&&) = default;
  message_handler(const message_handler&) = default;
  message_handler& operator=(message_handler&&) = default;
  message_handler& operator=(const message_handler&) = default;

  /// A pointer to the underlying implementation.
  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  /// Returns a pointer to the implementation.
  const impl_ptr& as_behavior_impl() const {
    return impl_;
  }

  /// Creates a message handler from @p ptr.
  message_handler(impl_ptr ptr);

  /// Checks whether the message handler is not empty.
  operator bool() const {
    return static_cast<bool>(impl_);
  }

  /// Create a message handler a list of match expressions,
  /// functors, or other message handlers.
  template <class T, class... Ts>
  message_handler(const T& v, Ts&&... xs) {
    assign(v, std::forward<Ts>(xs)...);
  }

  /// Assigns new message handlers.
  template <class... Ts>
  void assign(Ts... xs) {
    static_assert(sizeof...(Ts) > 0, "assign without arguments called");
    static_assert(!(may_have_timeout_v<std::decay_t<Ts>> || ...),
                  "Timeouts are only allowed in behaviors");
    impl_ = detail::make_behavior(xs...);
  }

  /// Equal to `*this = other`.
  void assign(message_handler what);

  /// Runs this handler and returns its (optional) result.
  std::optional<message> operator()(message& arg) {
    return (impl_) ? impl_->invoke(arg) : std::nullopt;
  }

  /// Runs this handler with callback.
  bool operator()(detail::invoke_result_visitor& f, message& xs) {
    return impl_ ? impl_->invoke(f, xs) : false;
  }

  /// Returns a new handler that concatenates this handler
  /// with a new handler from `xs...`.
  template <class... Ts>
  std::conditional_t<(may_have_timeout_v<std::decay_t<Ts>> || ...), behavior,
                     message_handler>
  or_else(Ts&&... xs) const {
    // using a behavior is safe here, because we "cast"
    // it back to a message_handler when appropriate
    behavior tmp{std::forward<Ts>(xs)...};
    if (!tmp) {
      return *this;
    }
    if (impl_)
      return impl_->or_else(tmp.as_behavior_impl());
    return tmp.as_behavior_impl();
  }

  /// @cond

  message_handler& unbox() {
    return *this;
  }

  /// @endcond

private:
  impl_ptr impl_;
};

} // namespace caf

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_MESSAGE_HANDLER_HPP
#define CAF_MESSAGE_HANDLER_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/on.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/ref_counted.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/behavior_impl.hpp"

namespace caf {

/// A partial function implementation used to process a `message`.
class message_handler {
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
  inline const impl_ptr& as_behavior_impl() const {
    return impl_;
  }

  /// Creates a message handler from @p ptr.
  message_handler(impl_ptr ptr);

  /// Checks whether the message handler is not empty.
  inline operator bool() const {
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
    impl_ = detail::make_behavior(xs...);
  }

  /// Equal to `*this = other`.
  void assign(message_handler other);

  /// Runs this handler and returns its (optional) result.
  inline optional<message> operator()(message& arg) {
    return (impl_) ? impl_->invoke(arg) : none;
  }

  /// Returns a new handler that concatenates this handler
  /// with a new handler from `xs...`.
  template <class... Ts>
  typename std::conditional<
    detail::disjunction<may_have_timeout<
      typename std::decay<Ts>::type>::value...
    >::value,
    behavior,
    message_handler
  >::type
  or_else(Ts&&... xs) const {
    // using a behavior is safe here, because we "cast"
    // it back to a message_handler when appropriate
    behavior tmp{std::forward<Ts>(xs)...};
    if (! tmp) {
      return *this;
    }
    if (impl_) {
      return impl_->or_else(tmp.as_behavior_impl());
    }
    return tmp.as_behavior_impl();
  }

  /// @cond PRIVATE

  inline message_handler& unbox() {
    return *this;
  }

  /// @endcond

private:
  impl_ptr impl_;
};

} // namespace caf

#endif // CAF_MESSAGE_HANDLER_HPP

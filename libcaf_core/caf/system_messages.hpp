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

#ifndef CAF_SYSTEM_MESSAGES_HPP
#define CAF_SYSTEM_MESSAGES_HPP

#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/group.hpp"
#include "caf/actor_addr.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/meta/type_name.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

/// Sent to all links when an actor is terminated.
/// @note Actors can override the default handler by calling
///       `self->set_exit_handler(...)`.
class exit_msg {
public:
  // -- friend types that need access to private ctors -------------------------

  template <class>
  friend class data_processor;

  template <class>
  friend class detail::type_erased_value_impl;

  // -- constructors -----------------------------------------------------------

  inline exit_msg(actor_addr x, error y)
      : source(std::move(x)),
        reason(std::move(y)) {
    // nop
  }

  // -- data members -----------------------------------------------------------

  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;

private:
  exit_msg() = default;
};

/// @relates exit_msg
template <class Inspector>
error inspect(Inspector& f, exit_msg& x) {
  return f(meta::type_name("exit_msg"), x.source, x.reason);
}

/// Sent to all actors monitoring an actor when it is terminated.
class down_msg {
public:
  // -- friend types that need access to private ctors -------------------------

  template <class>
  friend class data_processor;

  template <class>
  friend class detail::type_erased_value_impl;

  // -- constructors -----------------------------------------------------------

  inline down_msg(actor_addr x, error y)
      : source(std::move(x)),
        reason(std::move(y)) {
    // nop
  }

  // -- data members -----------------------------------------------------------

  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;

private:
  down_msg() = default;
};

/// @relates down_msg
template <class Inspector>
error inspect(Inspector& f, down_msg& x) {
  return f(meta::type_name("down_msg"), x.source, x.reason);
}

/// Sent to all members of a group when it goes offline.
struct group_down_msg {
  /// The source of this message, i.e., the now unreachable group.
  group source;
};

/// @relates group_down_msg
template <class Inspector>
error inspect(Inspector& f, group_down_msg& x) {
  return f(meta::type_name("group_down_msg"), x.source);
}

/// Signalizes a timeout event.
/// @note This message is handled implicitly by the runtime system.
struct timeout_msg {
  /// Actor-specific timeout ID.
  uint32_t timeout_id;
};

/// @relates timeout_msg
template <class Inspector>
error inspect(Inspector& f, timeout_msg& x) {
  return f(meta::type_name("timeout_msg"), x.timeout_id);
}

} // namespace caf

#endif // CAF_SYSTEM_MESSAGES_HPP

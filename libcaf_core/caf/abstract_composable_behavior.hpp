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

#ifndef CAF_ABSTRACT_COMPOSABLE_STATE_HPP
#define CAF_ABSTRACT_COMPOSABLE_STATE_HPP

#include <utility>

#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/message.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/pseudo_tuple.hpp"

namespace caf {

/// Marker type that allows CAF to spawn actors from composable states.
class abstract_composable_behavior {
public:
  virtual ~abstract_composable_behavior();

  virtual void init_behavior(behavior& x) = 0;

  template <class Derived, class... Ts>
  auto invoke_mutable(Derived* thisptr, const Ts&...)
  -> decltype(std::declval<Derived>()(std::declval<Ts>()...)) {
    // re-dispatch on current message via lambda expression
    auto f = [thisptr](Ts&... xs) {
      return (*thisptr)(xs...);
    };
    // reference to the current message
    auto& msg = thisptr->self->current_message();
    if (! msg.template match_elements<Ts...>())
      return sec::invalid_invoke_mutable;
    // make sure no other actor accesses the same data
    msg.force_unshare();
    // fill a pseudo tuple with values and invoke f
    detail::pseudo_tuple<Ts...> buf;
    // msg is guaranteed to be detached, hence we don't need to
    // check this condition over and over again via get_mutable
    for (size_t i = 0; i < msg.size(); ++i)
      buf[i] = const_cast<void*>(msg.at(i));
    return detail::apply_args(f, detail::get_indices(buf), buf);
  }
};

} // namespace caf

#endif // CAF_ABSTRACT_COMPOSABLE_STATE_HPP

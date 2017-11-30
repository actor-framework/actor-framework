/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP
#define CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP

#include <cstdint>

#include "caf/error.hpp"
#include "caf/fwd.hpp"

#include "caf/intrusive/task_result.hpp"

namespace caf {
namespace detail {

struct sync_request_bouncer {
  error rsn;
  explicit sync_request_bouncer(error r);
  void operator()(const strong_actor_ptr& sender, const message_id& mid) const;
  void operator()(const mailbox_element& e) const;

  template <class Key, class Queue>
  intrusive::task_result operator()(const Key&, const Queue&,
                                    const mailbox_element& x) const {
    (*this)(x);
    return intrusive::task_result::resume;
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_MIXIN_SINGLE_TIMEOUT_HPP
#define CAF_MIXIN_SINGLE_TIMEOUT_HPP

#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/system_messages.hpp"

namespace caf {
namespace mixin {

/**
 * Mixin for actors using a non-nestable message processing.
 */
template <class Base, class Subtype>
class single_timeout : public Base {
 public:
  using super = Base;

  using combined_type = single_timeout;

  template <class... Ts>
  single_timeout(Ts&&... args)
      : super(std::forward<Ts>(args)...),
        m_timeout_id(0) {
    // nop
  }

  void request_timeout(const duration& d) {
    if (d.valid()) {
      this->has_timeout(true);
      auto tid = ++m_timeout_id;
      auto msg = make_message(timeout_msg{tid});
      if (d.is_zero()) {
        // immediately enqueue timeout message if duration == 0s
        this->enqueue(this->address(), invalid_message_id,
                      std::move(msg), this->host());
      } else
        this->delayed_send_tuple(this, d, std::move(msg));
    } else
      this->has_timeout(false);
  }

  bool waits_for_timeout(uint32_t timeout_id) const {
    return this->has_timeout() && m_timeout_id == timeout_id;
  }

  bool is_active_timeout(uint32_t tid) const {
    return waits_for_timeout(tid);
  }

  uint32_t active_timeout_id() const {
    return m_timeout_id;
  }

  void reset_timeout() {
    this->has_timeout(false);
  }

 protected:
  uint32_t m_timeout_id;
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SINGLE_TIMEOUT_HPP

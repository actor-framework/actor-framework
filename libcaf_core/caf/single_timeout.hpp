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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_SINGLE_TIMEOUT_HPP
#define CAF_SINGLE_TIMEOUT_HPP

#include "caf/message.hpp"
#include "caf/system_messages.hpp"

#include "caf/duration.hpp"

namespace caf {

/**
 * @brief Mixin for actors using a non-nestable message processing.
 */
template <class Base, class Subtype>
class single_timeout : public Base {

  using super = Base;

 public:

  using combined_type = single_timeout;

  template <class... Ts>
  single_timeout(Ts&&... args)
      : super(std::forward<Ts>(args)...), m_has_timeout(false)
      , m_timeout_id(0) { }

  void request_timeout(const duration& d) {
    if (d.valid()) {
      m_has_timeout = true;
      auto tid = ++m_timeout_id;
      auto msg = make_message(timeout_msg{tid});
      if (d.is_zero()) {
        // immediately enqueue timeout message if duration == 0s
        this->enqueue(this->address(),
                message_id::invalid,
                std::move(msg),
                this->m_host);
        //auto e = this->new_mailbox_element(this, std::move(msg));
        //this->m_mailbox.enqueue(e);
      }
      else this->delayed_send_tuple(this, d, std::move(msg));
    }
    else m_has_timeout = false;
  }

  inline bool waits_for_timeout(std::uint32_t timeout_id) const {
    return m_has_timeout && m_timeout_id == timeout_id;
  }

  inline bool is_active_timeout(std::uint32_t tid) const {
    return waits_for_timeout(tid);
  }

  inline bool has_active_timeout() const {
    return m_has_timeout;
  }

  inline void reset_timeout() {
    m_has_timeout = false;
  }

 protected:

  bool m_has_timeout;
  std::uint32_t m_timeout_id;

};

} // namespace caf

#endif // CAF_SINGLE_TIMEOUT_HPP

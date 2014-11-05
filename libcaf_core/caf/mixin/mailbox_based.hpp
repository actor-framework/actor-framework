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

#ifndef CAF_MIXIN_MAILBOX_BASED_HPP
#define CAF_MIXIN_MAILBOX_BASED_HPP

#include <type_traits>

#include "caf/mailbox_element.hpp"

#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace mixin {

template <class Base, class Subtype>
class mailbox_based : public Base {
 public:
  using del = detail::disposer;
  using mailbox_type = detail::single_reader_queue<mailbox_element, del>;

  ~mailbox_based() {
    if (!m_mailbox.closed()) {
      detail::sync_request_bouncer f{this->exit_reason()};
      m_mailbox.close(f);
    }
  }

  void cleanup(uint32_t reason) {
    detail::sync_request_bouncer f{reason};
    m_mailbox.close(f);
    Base::cleanup(reason);
  }

  mailbox_type& mailbox() {
    return m_mailbox;
  }

 protected:
  using combined_type = mailbox_based;

  template <class... Ts>
  mailbox_based(Ts&&... args) : Base(std::forward<Ts>(args)...) {
    // nop
  }

  mailbox_type m_mailbox;
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_MAILBOX_BASED_HPP

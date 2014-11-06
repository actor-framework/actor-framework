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

#ifndef CAF_POLICY_MIDDLEMAN_SCHEDULING_HPP
#define CAF_POLICY_MIDDLEMAN_SCHEDULING_HPP

#include <utility>

#include "caf/singletons.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/io/middleman.hpp"

#include "caf/policy/cooperative_scheduling.hpp"

namespace caf {
namespace policy {

// Actor must implement invoke_message
class middleman_scheduling {

 public:

  template <class Actor>
  class continuation {

   public:

    using pointer = intrusive_ptr<Actor>;

    continuation(pointer ptr, msg_hdr_cref hdr, message&& msg)
    : m_self(std::move(ptr)), m_hdr(hdr), m_data(std::move(msg)) { }

    inline void operator()() const {
      m_self->invoke_message(m_hdr, std::move(m_data));
    }

   private:

    pointer    m_self;
    message_header m_hdr;
    message    m_data;

  };

  using timeout_type = int;

  // this does return nullptr
  template <class Actor, typename F>
  inline void fetch_messages(Actor*, F) {
    // clients cannot fetch messages
  }

  template <class Actor, typename F>
  inline void fetch_messages(Actor* self, F cb, timeout_type) {
    // a call to this call is always preceded by init_timeout,
    // which will trigger a timeout message
    fetch_messages(self, cb);
  }

  template <class Actor>
  inline void launch(Actor*) {
    // nothing to do
  }

  template <class Actor>
  void enqueue(Actor* self, msg_hdr_cref hdr, message& msg) {
    get_middleman()->run_later(continuation<Actor>{self, hdr, std::move(msg)});
  }

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_MIDDLEMAN_SCHEDULING_HPP

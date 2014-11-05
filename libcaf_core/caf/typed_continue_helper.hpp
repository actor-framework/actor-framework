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

#ifndef CAF_TYPED_CONTINUE_HELPER_HPP
#define CAF_TYPED_CONTINUE_HELPER_HPP

#include "caf/continue_helper.hpp"

#include "caf/detail/type_traits.hpp"

#include "caf/detail/typed_actor_util.hpp"

namespace caf {

template <class OutputList>
class typed_continue_helper {
 public:
  using message_id_wrapper_tag = int;
  using getter = std::function<optional<behavior&> (message_id msg_id)>;

  typed_continue_helper(message_id mid, getter get_sync_handler)
      : m_ch(mid, std::move(get_sync_handler)) {
    // nop
  }

  typed_continue_helper(continue_helper ch) : m_ch(std::move(ch)) {
    // nop
  }

  template <class F>
  typed_continue_helper<typename detail::get_callable_trait<F>::result_type>
  continue_with(F fun) {
    detail::assert_types<OutputList, F>();
    m_ch.continue_with(std::move(fun));
    return {m_ch};
  }

  message_id get_message_id() const {
    return m_ch.get_message_id();
  }

 private:
  continue_helper m_ch;
};

} // namespace caf

#endif // CAF_TYPED_CONTINUE_HELPER_HPP

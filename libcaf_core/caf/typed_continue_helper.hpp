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

  typed_continue_helper(message_id mid) : ch_(mid) {
    // nop
  }

  typed_continue_helper(continue_helper ch) : ch_(std::move(ch)) {
    // nop
  }

  message_id get_message_id() const {
    return ch_.get_message_id();
  }

private:
  continue_helper ch_;
};

} // namespace caf

#endif // CAF_TYPED_CONTINUE_HELPER_HPP

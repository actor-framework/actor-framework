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

#ifndef CONTINUE_HELPER_HPP
#define CONTINUE_HELPER_HPP

#include <functional>

#include "caf/on.hpp"
#include "caf/behavior.hpp"
#include "caf/message_id.hpp"
#include "caf/message_handler.hpp"

namespace caf {

class local_actor;

/// Helper class to enable users to add continuations
///  when dealing with synchronous sends.
class continue_helper {
public:
  using message_id_wrapper_tag = int;
  continue_helper(message_id mid);

  /// Returns the ID of the expected response message.
  message_id get_message_id() const {
    return mid_;
  }

private:
  message_id mid_;
};

} // namespace caf

#endif // CONTINUE_HELPER_HPP

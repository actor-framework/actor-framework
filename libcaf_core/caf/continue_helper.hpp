/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_CONTINUE_HELPER_HPP
#define CAF_CONTINUE_HELPER_HPP

#include "caf/message_id.hpp"

namespace caf {

/// Helper class to enable users to add continuations
///  when dealing with synchronous sends.
class continue_helper {
public:
  explicit continue_helper(message_id mid);

  /// Returns the ID of the expected response message.
  message_id get_message_id() const {
    return mid_;
  }

private:
  message_id mid_;
};

} // namespace caf

#endif // CAF_CONTINUE_HELPER_HPP

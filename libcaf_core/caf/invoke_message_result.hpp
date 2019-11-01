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

#pragma once

#include <string>

namespace caf {

/// Stores the result of a message invocation.
enum class invoke_message_result {
  /// Indicates that the actor consumed the message.
  consumed,

  /// Indicates that the actor left the message in the mailbox.
  skipped,

  /// Indicates that the actor discarded the message based on meta data. For
  /// example, timeout messages for already received requests usually get
  /// dropped without calling any user-defined code.
  dropped,
};

/// @relates invoke_message_result
std::string to_string(invoke_message_result);

} // namespace caf

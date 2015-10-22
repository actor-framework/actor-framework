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

#ifndef CAF_MIXIN_SYNC_SENDER_HPP
#define CAF_MIXIN_SYNC_SENDER_HPP

#include "caf/mixin/request_sender.hpp"

namespace caf {
namespace mixin {

// <backward_compatibility version="0.14.2">
template <class ResponseHandleTag>
class CAF_DEPRECATED sync_sender {
public:
  template <class Base, class Subtype>
  using impl = request_sender_impl<Base, Subtype, ResponseHandleTag>;
};
// </backward_compatibility>

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SYNC_SENDER_HPP

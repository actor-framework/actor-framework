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

#ifndef CAF_ACCEPT_HANDLE_HPP
#define CAF_ACCEPT_HANDLE_HPP

#include "caf/io/handle.hpp"

namespace caf {
namespace io {

/// Generic handle type for managing incoming connections.
class accept_handle : public handle<accept_handle> {

  friend class handle<accept_handle>;

  using super = handle<accept_handle>;

public:

  accept_handle() = default;

private:

  inline accept_handle(int64_t handle_id) : super{handle_id} {}

};

} // namespace ios
} // namespace caf

#endif // CAF_ACCEPT_HANDLE_HPP

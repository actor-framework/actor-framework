/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/socket_manager.hpp"

#include "caf/config.hpp"
#include "caf/net/multiplexer.hpp"

namespace caf {
namespace net {

socket_manager::socket_manager(socket handle, const multiplexer_ptr& parent)
  : handle_(handle), mask_(operation::none), parent_(parent) {
  CAF_ASSERT(parent != nullptr);
  CAF_ASSERT(handle_ != invalid_socket);
}

socket_manager::~socket_manager() {
  close(handle_);
}

operation socket_manager::mask() const noexcept {
  return mask_.load();
}

bool socket_manager::mask_add(operation flag) noexcept {
  CAF_ASSERT(flag != operation::none);
  auto x = mask();
  while ((x & flag) != flag)
    if (mask_.compare_exchange_strong(x, x | flag)) {
      if (auto ptr = parent_.lock())
        ptr->update(this);
      return true;
    }
  return false;
}

bool socket_manager::mask_del(operation flag) noexcept {
  CAF_ASSERT(flag != operation::none);
  auto x = mask();
  while ((x & flag) != operation::none)
    if (mask_.compare_exchange_strong(x, x & ~flag)) {
      if (auto ptr = parent_.lock())
        ptr->update(this);
      return true;
    }
  return false;
}

} // namespace net
} // namespace caf

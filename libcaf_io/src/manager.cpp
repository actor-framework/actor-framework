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

#include "caf/io/network/manager.hpp"

#include "caf/detail/logging.hpp"

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {
namespace network {

manager::manager(abstract_broker* ptr) : parent_(ptr) {
  // nop
}

manager::~manager() {
  // nop
}

void manager::set_parent(abstract_broker* ptr) {
  if (! detached())
    parent_ = ptr;
}

void manager::detach(bool invoke_disconnect_message) {
  CAF_LOG_TRACE("");
  if (! detached()) {
    CAF_LOG_DEBUG("disconnect servant from broker");
    auto ptr = parent();
    set_parent(nullptr);
    detach_from(ptr);
    if (invoke_disconnect_message) {
      auto mptr = mailbox_element::make(invalid_actor_addr, invalid_message_id,
                                        detach_message());
      ptr->exec_single_event(mptr);
    }
  }
}

} // namespace network
} // namespace io
} // namespace caf

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

#include "caf/abstract_channel.hpp"

#include "caf/mailbox_element.hpp"
#include "caf/detail/singletons.hpp"

namespace caf {

using detail::singletons;

abstract_channel::abstract_channel(int init_flags)
    : flags_(init_flags),
      node_(singletons::get_node_id()) {
  // nop
}

abstract_channel::abstract_channel(int init_flags, node_id nid)
    : flags_(init_flags),
      node_(std::move(nid)) {
  // nop
}

abstract_channel::~abstract_channel() {
  // nop
}

void abstract_channel::enqueue(mailbox_element_ptr what, execution_unit* host) {
  enqueue(what->sender, what->mid, what->msg, host);
}

bool abstract_channel::is_remote() const {
  return node_ != singletons::get_node_id();
}

} // namespace caf

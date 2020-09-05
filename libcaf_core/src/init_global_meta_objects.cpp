/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/init_global_meta_objects.hpp"

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/config_value.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/error.hpp"
#include "caf/group.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv4_subnet.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/ipv6_endpoint.hpp"
#include "caf/ipv6_subnet.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"
#include "caf/system_messages.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"
#include "caf/unit.hpp"
#include "caf/upstream_msg.hpp"
#include "caf/uri.hpp"

namespace caf::core {

void init_global_meta_objects() {
  caf::init_global_meta_objects<id_block::core_module>();
}

} // namespace caf::core

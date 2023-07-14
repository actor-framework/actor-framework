// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/init_global_meta_objects.hpp"

#include "caf/action.hpp"
#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/config_value.hpp"
#include "caf/cow_string.hpp"
#include "caf/error.hpp"
#include "caf/group.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv4_subnet.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/ipv6_endpoint.hpp"
#include "caf/ipv6_subnet.hpp"
#include "caf/json_array.hpp"
#include "caf/json_object.hpp"
#include "caf/json_value.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"
#include "caf/stream.hpp"
#include "caf/system_messages.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"
#include "caf/unit.hpp"
#include "caf/uri.hpp"
#include "caf/uuid.hpp"

namespace caf::core {

void init_global_meta_objects() {
  caf::init_global_meta_objects<id_block::core_module>();
}

} // namespace caf::core

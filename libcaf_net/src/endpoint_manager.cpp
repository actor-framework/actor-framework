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

#include "caf/net/endpoint_manager.hpp"

#include "caf/intrusive/inbox_result.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"

namespace caf {
namespace net {

endpoint_manager::event::event(std::string path, actor listener)
  : value(resolve_request{std::move(path), std::move(listener)}) {
  // nop
}

endpoint_manager::event::event(atom_value type, uint64_t id)
  : value(timeout{type, id}) {
  // nop
}

endpoint_manager::endpoint_manager(socket handle, const multiplexer_ptr& parent,
                                   actor_system& sys)
  : super(handle, parent),
    sys_(sys),
    events_(event_policy{}),
    messages_(message_policy{}) {
  // nop
}

endpoint_manager::~endpoint_manager() {
  // nop
}

void endpoint_manager::resolve(std::string path, actor listener) {
  using intrusive::inbox_result;
  auto ptr = new event(std::move(path), std::move(listener));
  switch (events_.push_back(ptr)) {
    default:
      break;
    case inbox_result::unblocked_reader:
      mask_add(operation::write);
      break;
    case inbox_result::queue_closed:
      anon_send(listener, resolve_atom::value,
                make_error(sec::request_receiver_down));
  }
}

} // namespace net
} // namespace caf

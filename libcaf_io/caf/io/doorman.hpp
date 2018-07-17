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

#include <cstddef>
#include <cstdint>

#include "caf/message.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/io/accept_handle.hpp"
#include "caf/io/broker_servant.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/network/acceptor_manager.hpp"

namespace caf {
namespace io {

using doorman_base = broker_servant<network::acceptor_manager, accept_handle,
                                    new_connection_msg>;

/// Manages incoming connections.
/// @ingroup Broker
class doorman : public doorman_base {
public:
  doorman(accept_handle acc_hdl);

  ~doorman() override;

  using doorman_base::new_connection;

  bool new_connection(execution_unit* ctx, connection_handle x);

  /// Starts listening on the selected port.
  virtual void launch() = 0;

protected:
  message detach_message() override;
};

using doorman_ptr = intrusive_ptr<doorman>;

} // namespace io
} // namespace caf

// Allows the `middleman_actor` to create a `doorman` and then send it to the
// BASP broker.
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::io::doorman_ptr)


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

#ifndef CAF_IO_DOORMAN_HPP
#define CAF_IO_DOORMAN_HPP

#include <cstddef>

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
  doorman(abstract_broker* parent, accept_handle hdl);

  ~doorman();

  void io_failure(network::operation op) override;

  // needs to be launched explicitly
  virtual void launch() = 0;

protected:
  message detach_message() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_DOORMAN_HPP

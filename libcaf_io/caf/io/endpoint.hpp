/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_IO_ENDPOINT_HPP
#define CAF_IO_ENDPOINT_HPP

#include <vector>

#include "caf/message.hpp"

#include "caf/io/broker_servant.hpp"
#include "caf/io/endpoint_handle.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/network/endpoint_manager.hpp"

namespace caf {
namespace io {

using endpoint_base = broker_servant<network::endpoint_manager, endpoint_handle,
                                     datagram_msg>;

/// Manages writing and reading on a datagram endpoint.
/// @ingroup Broker
class endpoint : public endpoint_base {
public:
  endpoint(abstract_broker* parent, endpoint_handle hdl);

  ~endpoint();

  /// Enables or disables write notifications.
  virtual void ack_writes(bool enable) = 0;

  /// Configure buffer size for next accepted datagram.
  /// Implicitly starts the read loop on first call.
  virtual void configure_datagram_size(size_t buf_size) = 0;

  /// Returns the current write buffer.
  virtual std::vector<char>& wr_buf() = 0;

  /// Returns the current input buffer.
  virtual std::vector<char>& rd_buf() = 0;

  bool consume(execution_unit* ctx, const void* buf, size_t besize) override;
  
  void datagram_sent(execution_unit* ctx, size_t num_bytes) override;

  void io_failure(execution_unit* ctx, network::operation op) override;

  // needs to be launched explicitly, TODO: Does it?
  // Can't configure_datagram_size do that?
  virtual void launch() = 0;

protected:
  message detach_message() override;
};


} // namespace io
} // namespace caf

#endif // CAF_IO_ENDPOINT_HPP

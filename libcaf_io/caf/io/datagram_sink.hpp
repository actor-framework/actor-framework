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

#ifndef CAF_IO_DATAGRAM_SINK_HPP
#define CAF_IO_DATAGRAM_SINK_HPP

#include <vector>

#include "caf/message.hpp"

#include "caf/io/broker_servant.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/datagram_sink_handle.hpp"
#include "caf/io/network/datagram_sink_manager.hpp"

namespace caf {
namespace io {

using datagram_sink_base = broker_servant<network::datagram_sink_manager,
                                          datagram_sink_handle,
                                          datagram_sink_msg>;

/// Manages writing to a datagram sink.
/// @ingroup Broker
class datagram_sink : public datagram_sink_base {
public:
  datagram_sink(abstract_broker* parent, datagram_sink_handle hdl);

  ~datagram_sink();

  /// Enables or disables write notifications.
  virtual void ack_writes(bool enable) = 0;

  /// Returns the current write buffer.
  virtual std::vector<char>& wr_buf() = 0;

  void datagram_sent(execution_unit* ctx, size_t num_bytes) override;

  void io_failure(execution_unit* ctx, network::operation op) override;

protected:
  message detach_message() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_DATAGRAM_SINK_HPP


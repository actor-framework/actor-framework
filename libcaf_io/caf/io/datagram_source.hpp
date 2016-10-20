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

#ifndef CAF_IO_DATAGRAM_SOURCE_HPP
#define CAF_IO_DATAGRAM_SOURCE_HPP

#include <vector>

#include "caf/message.hpp"

#include "caf/io/broker_servant.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/datagram_source_handle.hpp"
#include "caf/io/network/datagram_source_manager.hpp"

namespace caf {
namespace io {

using datagram_source_base = broker_servant<network::datagram_source_manager,
                                            datagram_source_handle,
                                            new_datagram_msg>;

/// Manages reading from a datagram source
/// @ingroup Broker
class datagram_source : public datagram_source_base {
public:
  datagram_source(abstract_broker* parent, datagram_source_handle hdl);

  ~datagram_source();

  /*
  /// Implicitly starts the read loop on first call.
  virtual void configure_read(receive_policy::config config) = 0;
  */

  /// Returns the current input buffer.
  virtual std::vector<char>& rd_buf() = 0;

  bool consume(execution_unit* ctx, const void* buf, size_t besize) override;

  void io_failure(execution_unit* ctx, network::operation op) override;

protected:
  message detach_message() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_DATAGRAM_SOURCE_HPP


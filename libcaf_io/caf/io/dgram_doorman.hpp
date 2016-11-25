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

#ifndef CAF_IO_DGRAM_DOORMAN_HPP
#define CAF_IO_DGRAM_DOORMAN_HPP

#include <vector>

#include "caf/message.hpp"

#include "caf/io/broker_servant.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/dgram_doorman_handle.hpp"
#include "caf/io/network/dgram_acceptor_manager.hpp"

namespace caf {
namespace io {

using dgram_doorman_base = broker_servant<network::dgram_acceptor_manager,
                                          dgram_doorman_handle,
                                          dgram_dummy_msg>;

/// Manages reading from a datagram source
/// @ingroup Broker
class dgram_doorman : public dgram_doorman_base {
public:
  dgram_doorman(abstract_broker* parent, dgram_doorman_handle hdl);

  ~dgram_doorman();

  /// Configure buffer size for next accepted datagram.
  virtual void configure_datagram_size(size_t buf_size) = 0;

  /// Returns the current input buffer.
  virtual std::vector<char>& rd_buf() = 0;

  /// Returns the local port
  virtual uint16_t local_port() const = 0;

  void io_failure(execution_unit* ctx, network::operation op) override;

  using dgram_doorman_base::new_endpoint;

  bool new_endpoint(execution_unit* ctx, dgram_scribe_handle endpoint,
                    const void* buf, size_t num_bytes);

  // needs to be launched explicitly
  virtual void launch() = 0;

protected:
  message detach_message() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_DGRAM_DOORMAN_HPP

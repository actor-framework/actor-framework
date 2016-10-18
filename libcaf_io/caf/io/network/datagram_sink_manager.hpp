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

#ifndef CAF_IO_NETWORK_DATAGRAM_SINK_MANGER_HPP
#define CAF_IO_NETWORK_DATAGRAM_SINK_MANGER_HPP

#include "caf/io/network/manager.hpp"

namespace caf {
namespace io {
namespace network {

/// A datagram manager provides callbacks for outgoing
/// datagrams as well as for error handling.
class datagram_sink_manager : public manager {
public:
  datagram_sink_manager(abstract_broker* ptr);

  ~datagram_sink_manager();

  /// Called by the underlying I/O device whenever it sent a datagram.
  virtual void datagram_sent(execution_unit* ctx, size_t num_bytes) = 0;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_DATAGRAM_SINK_MANGER_HPP

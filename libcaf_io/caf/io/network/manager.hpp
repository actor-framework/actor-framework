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

#ifndef CAF_IO_NETWORK_MANAGER_HPP
#define CAF_IO_NETWORK_MANAGER_HPP

#include "caf/message.hpp"
#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/network/operation.hpp"

namespace caf {
namespace io {
namespace network {

/// A manager configures an IO device and provides callbacks
/// for various IO operations.
class manager : public ref_counted {
public:
  manager(abstract_broker* parent_ptr);

  ~manager();

  /// Sets the parent for this manager.
  /// @pre `parent() == nullptr`
  void set_parent(abstract_broker* ptr);

  /// Returns the parent broker of this manager.
  inline abstract_broker* parent() {
    return parent_;
  }

  /// Returns `true` if this manager has a parent, `false` otherwise.
  inline bool detached() const {
    return parent_ == nullptr;
  }

  /// Detach this manager from its parent and invoke `detach_message()``
  /// if `invoke_detach_message == true`.
  void detach(bool invoke_detach_message);

  /// Causes the manager to stop read operations on its IO device.
  /// Unwritten bytes are still send before the socket will be closed.
  virtual void stop_reading() = 0;

  /// Called by the underlying IO device to report failures.
  virtual void io_failure(operation op) = 0;

  /// Get the address of the underlying IO device.
  virtual std::string addr() const = 0;

  /// Get the port of the underlying IO device.
  virtual uint16_t port() const = 0;

protected:
  /// Creates a message signalizing a disconnect to the parent.
  virtual message detach_message() = 0;

  /// Detaches this manager from `ptr`.
  virtual void detach_from(abstract_broker* ptr) = 0;

private:
  abstract_broker* parent_;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_MANAGER_HPP

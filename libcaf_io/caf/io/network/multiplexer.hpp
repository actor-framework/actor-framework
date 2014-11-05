/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_IO_NETWORK_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_MULTIPLEXER_HPP

#include <string>
#include <thread>
#include <functional>

#include "caf/extend.hpp"
#include "caf/memory_managed.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/network/native_socket.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/detail/memory.hpp"

namespace boost {
namespace asio {
class io_service;
} // namespace asio
} // namespace boost

namespace caf {
namespace io {
namespace network {

/**
 * Low-level backend for IO multiplexing.
 */
class multiplexer {
 public:
  virtual ~multiplexer();

  /**
   * Creates a new TCP doorman from a native socket handle.
   */
  virtual connection_handle add_tcp_scribe(broker* ptr, native_socket fd) = 0;

  /**
   * Tries to connect to host `h` on given `port` and returns a
   * new scribe managing the connection on success.
   */
  virtual connection_handle add_tcp_scribe(broker* ptr, const std::string& host,
                                           uint16_t port) = 0;

  /**
   * Creates a new TCP doorman from a native socket handle.
   */
  virtual accept_handle add_tcp_doorman(broker* ptr, native_socket fd) = 0;

  /**
   * Tries to create a new TCP doorman running on port `p`, optionally
   * accepting only connections from IP address `in`.
   */
  virtual accept_handle add_tcp_doorman(broker* ptr, uint16_t port,
                                        const char* in = nullptr) = 0;

  /**
   * Simple wrapper for runnables
   */
  struct runnable : extend<memory_managed>::with<mixin::memory_cached> {
    virtual void run() = 0;
    virtual ~runnable();
  };

  using runnable_ptr = std::unique_ptr<runnable, detail::disposer>;

  /**
   * Makes sure the multipler does not exit its event loop until
   * the destructor of `supervisor` has been called.
   */
  struct supervisor {
   public:
    virtual ~supervisor();
  };

  using supervisor_ptr = std::unique_ptr<supervisor>;

  /**
   * Creates a supervisor to keep the event loop running.
   */
  virtual supervisor_ptr make_supervisor() = 0;

  /**
   * Creates an instance using the networking backend compiled with CAF.
   */
  static std::unique_ptr<multiplexer> make();

  /**
   * Runs the multiplexers event loop.
   */
  virtual void run() = 0;

  /**
   * Invokes @p fun in the multiplexer's event loop.
   */
  template <class F>
  void dispatch(F fun) {
    if (std::this_thread::get_id() == thread_id()) {
      fun();
      return;
    }
    post(std::move(fun));
  }

  /**
   * Invokes @p fun in the multiplexer's event loop.
   */
  template <class F>
  void post(F fun) {
    struct impl : runnable {
      F f;
      impl(F&& mf) : f(std::move(mf)) { }
      void run() override {
        f();
      }
    };
    dispatch_runnable(runnable_ptr{detail::memory::create<impl>(std::move(fun))});
  }

  /**
   * Retrieves a pointer to the implementation or `nullptr` if CAF was
   * compiled using the default backend.
   */
  virtual boost::asio::io_service* pimpl();

  inline const std::thread::id& thread_id() const {
    return m_tid;
  }

  inline void thread_id(std::thread::id tid) {
    m_tid = std::move(tid);
  }

 protected:
  /**
   * Implementation-specific dispatching to the multiplexer's thread.
   */
  virtual void dispatch_runnable(runnable_ptr ptr) = 0;

  /**
   * Must be set by the subclass.
   */
  std::thread::id m_tid;
};

using multiplexer_ptr = std::unique_ptr<multiplexer>;

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_MULTIPLEXER_HPP

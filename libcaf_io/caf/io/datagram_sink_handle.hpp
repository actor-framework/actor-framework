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

#ifndef CAF_IO_DATAGRAM_SINK_HANDLE_HPP
#define CAF_IO_DATAGRAM_SINK_HANDLE_HPP

#include <functional>

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {

struct invalid_datagram_sink_handle_t {
  constexpr invalid_datagram_sink_handle_t() {
    // nop
  }
};

constexpr invalid_datagram_sink_handle_t invalid_datagram_sink_handle
  = invalid_datagram_sink_handle_t{};


/// Generic type for identifying datagram sink.
class datagram_sink_handle : public handle<datagram_sink_handle,
                                           invalid_datagram_sink_handle_t> {
public:
  friend class handle<datagram_sink_handle, invalid_datagram_sink_handle_t>;

  using super = handle<datagram_sink_handle, invalid_datagram_sink_handle_t>;

  datagram_sink_handle() : port_{0} {
    // nop
  }

  datagram_sink_handle(const invalid_datagram_sink_handle_t&) : port_{0} {
    // nop
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 datagram_sink_handle& x) {
    return f(meta::type_name("datagram_sink_handle"), x.id_);
  }

  const std::string& host() const {
    return host_;
  }

  void set_host(std::string host) {
    host_ = move(host);
  }

  uint16_t port() const {
    return port_;
  }

  void set_port(uint16_t port) {
    port_ = port;
  }

private:
  inline datagram_sink_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
  std::string host_;
  uint16_t port_;
};

} // namespace io
} // namespace caf

namespace std {

template<>
struct hash<caf::io::datagram_sink_handle> {
  size_t operator()(const caf::io::datagram_sink_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std
#endif // CAF_IO_DATAGRAM_SINK_HANDLE_HPP

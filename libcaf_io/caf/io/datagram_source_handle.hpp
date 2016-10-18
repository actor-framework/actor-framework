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

#ifndef CAF_IO_DATAGRAM_SOURCE_HANDLE_HPP
#define CAF_IO_DATAGRAM_SOURCE_HANDLE_HPP

#include <functional>

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {

struct invalid_datagram_source_handle_t {
  constexpr invalid_datagram_source_handle_t() {
    // nop
  }
};

constexpr invalid_datagram_source_handle_t invalid_datagram_source_handle
  = invalid_datagram_source_handle_t{};


/// Generic type for identifying a datagram source.
class datagram_source_handle : public handle<datagram_source_handle,
                                             invalid_datagram_source_handle_t> {
public:
  friend class handle<datagram_source_handle, invalid_datagram_source_handle_t>;

  using super = handle<datagram_source_handle, invalid_datagram_source_handle_t>;

  constexpr datagram_source_handle() {
    // nop
  }

  constexpr datagram_source_handle(const invalid_datagram_source_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 datagram_source_handle& x) {
    return f(meta::type_name("datagram_source_handle"), x.id_);
  }

private:
  inline datagram_source_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
};

} // namespace io
} // namespace caf

namespace std {

template<>
struct hash<caf::io::datagram_source_handle> {
  size_t operator()(const caf::io::datagram_source_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std
#endif // CAF_IO_DATAGRAM_SOURCE_HANDLE_HPP

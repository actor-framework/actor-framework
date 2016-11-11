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

#ifndef CAF_IO_ENDPOINT_HANDLE_HPP
#define CAF_IO_ENDPOINT_HANDLE_HPP

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {

struct invalid_endpoint_handle_t {
  constexpr invalid_endpoint_handle_t() {
    // nop
  }
};

constexpr invalid_endpoint_handle_t invalid_endpoint_handle
  = invalid_endpoint_handle_t{};

/// Generic handle type for managing local and remote datagram endpoints.
class endpoint_handle : public handle<endpoint_handle,
                                      invalid_endpoint_handle_t> {
public:
  friend class handle<endpoint_handle, invalid_endpoint_handle_t>;

  using super = handle<endpoint_handle,invalid_endpoint_handle_t>;

  constexpr endpoint_handle() {
    // nop
  }

  constexpr endpoint_handle(const invalid_endpoint_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 endpoint_handle& x) {
    return f(meta::type_name("endpoint_handle"), x.id_);
  }

private:
  inline endpoint_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
};

} // namespace io
} // namespace caf

namespace std {

template<>
struct hash<caf::io::endpoint_handle> {
  size_t operator()(const caf::io::endpoint_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std

#endif // CAF_IO_ENDPOINT_HANDLE_HPP

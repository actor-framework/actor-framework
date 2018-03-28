/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <functional>

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {

struct invalid_datagram_handle_t {
  constexpr invalid_datagram_handle_t() {
    // nop
  }
};

constexpr invalid_datagram_handle_t invalid_datagram_handle
  = invalid_datagram_handle_t{};

/// Generic handle type for identifying datagram endpoints
class datagram_handle : public handle<datagram_handle,
                                      invalid_datagram_handle_t> {
public:
  friend class handle<datagram_handle, invalid_datagram_handle_t>;

  using super = handle<datagram_handle, invalid_datagram_handle_t>;

  constexpr datagram_handle() {
    // nop
  }

  constexpr datagram_handle(const invalid_datagram_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 datagram_handle& x) {
    return f(meta::type_name("datagram_handle"), x.id_);
  }

private:
  inline datagram_handle(int64_t handle_id) : super{handle_id} {
    // nop
  }
};

} // namespace io
} // namespace caf

namespace std {

template<>
struct hash<caf::io::datagram_handle> {
  size_t operator()(const caf::io::datagram_handle& hdl) const {
    return std::hash<int64_t>{}(hdl.id());
  }
};

} // namespace std


/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

struct addr_visitor {
  using result_type = std::string;
  addr_visitor(abstract_broker* ptr) : ptr_(ptr) { }
  template <class Handle>
  result_type operator()(const Handle& hdl) { return ptr_->remote_addr(hdl); }
  abstract_broker* ptr_;
};

struct port_visitor {
  using result_type = uint16_t;
  port_visitor(abstract_broker* ptr) : ptr_(ptr) { }
  template <class Handle>
  result_type operator()(const Handle& hdl) { return ptr_->remote_port(hdl); }
  abstract_broker* ptr_;
};

struct id_visitor {
  using result_type = int64_t;
  template <class Handle>
  result_type operator()(const Handle& hdl) { return hdl.id(); }
};

struct hash_visitor {
  using result_type = size_t;
  template <class Handle>
  result_type operator()(const Handle& hdl) const {
    std::hash<Handle> f;
    return f(hdl);
  }
};

} // namespace io
} // namespace caf


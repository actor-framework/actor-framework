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

#ifndef CAF_IO_VISITORS_HPP
#define CAF_IO_VISITORS_HPP

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

struct wr_buf_visitor {
  wr_buf_visitor(abstract_broker* ptr) : ptr{ptr} { /* nop */ }
  using result_type = std::vector<char>&;
  template <typename Handle>
  result_type operator()(const Handle& hdl) { return ptr->wr_buf(hdl); }
  abstract_broker* ptr;
};
  
struct flush_visitor {
  flush_visitor(abstract_broker* ptr) : ptr{ptr} { /* nop */ }
  using result_type = std::vector<char>&;
  template <typename Handle>
  result_type operator()(const Handle& hdl) { return ptr->flush(hdl); }
  abstract_broker* ptr;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_VISITORS_HPP


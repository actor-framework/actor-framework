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

#ifndef CAF_STREAM_SOURCE_HPP
#define CAF_STREAM_SOURCE_HPP

#include <memory>

#include "caf/extend.hpp"
#include "caf/stream_handler.hpp"
#include "caf/downstream_policy.hpp"

#include "caf/mixin/has_downstreams.hpp"

namespace caf {

class stream_source : public extend<stream_handler, stream_source>::
                             with<mixin::has_downstreams> {
public:
  stream_source(abstract_downstream* out_ptr);

  ~stream_source() override;

  bool done() const final;

  error downstream_demand(strong_actor_ptr& hdl, size_t value) final;

  void abort(strong_actor_ptr& cause, const error& reason) final;

  inline abstract_downstream& out() {
    return *out_ptr_;
  }

protected:
  /// Queries the current amount of elements in the output buffer.
  virtual size_t buf_size() const = 0;

  /// Generate new elements for the output buffer. The size hint `n` indicates
  /// how much elements can be shipped immediately.
  virtual void generate(size_t n) = 0;

  /// Queries whether the source is exhausted.
  virtual bool at_end() const = 0;

private:
  abstract_downstream* out_ptr_;
};

} // namespace caf

#endif // CAF_STREAM_SOURCE_HPP

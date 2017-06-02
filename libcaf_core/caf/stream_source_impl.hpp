/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_STREAM_SOURCE_IMPL_HPP
#define CAF_STREAM_SOURCE_IMPL_HPP

#include "caf/downstream.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {

template <class Fun, class Predicate, class DownstreamPolicy>
class stream_source_impl : public stream_source {
public:
  using trait = stream_source_trait_t<Fun>;

  using state_type = typename trait::state;

  using output_type = typename trait::output;

  stream_source_impl(local_actor* self, const stream_id& sid,
                     Fun fun, Predicate pred)
      : stream_source(&out_),
        fun_(std::move(fun)),
        pred_(std::move(pred)),
        out_(self, sid) {
    // nop
  }

  void generate(size_t num) override {
    CAF_LOG_TRACE(CAF_ARG(num));
    downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
    fun_(state_, ds, num);
  }

  long buf_size() const override {
    return out_.buf_size();
  }

  bool at_end() const override {
    return pred_(state_);
  }

  optional<downstream_policy&> dp() override {
    return out_;
  }

  state_type& state() {
    return state_;
  }

private:
  state_type state_;
  Fun fun_;
  Predicate pred_;
  DownstreamPolicy out_;
};

} // namespace caf

#endif // CAF_STREAM_SOURCE_IMPL_HPP

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

#ifndef CAF_STREAM_STAGE_IMPL_HPP
#define CAF_STREAM_STAGE_IMPL_HPP

#include "caf/downstream.hpp"
#include "caf/stream_stage.hpp"
#include "caf/stream_stage_trait.hpp"

#include "caf/policy/greedy.hpp"
#include "caf/policy/broadcast.hpp"

namespace caf {

template <class Fun, class Cleanup,
         class UpstreamPolicy, class DownstreamPolicy>
class stream_stage_impl : public stream_stage {
public:
  using trait = stream_stage_trait_t<Fun>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_stage_impl(local_actor* self,
                    const stream_id& sid,
                    Fun fun, Cleanup cleanup)
      : stream_stage(&in_, &out_),
        fun_(std::move(fun)),
        cleanup_(std::move(cleanup)),
        in_(self),
        out_(self, sid) {
    // nop
  }

  expected<long> add_upstream(strong_actor_ptr& ptr, const stream_id& sid,
                              stream_priority prio) override {
    CAF_LOG_TRACE(CAF_ARG(ptr) << CAF_ARG(sid) << CAF_ARG(prio));
    if (ptr)
      return in().add_path(ptr, sid, prio, out_.credit());
    return sec::invalid_argument;
  }

  error process_batch(message& msg) override {
    using vec_type = std::vector<output_type>;
    if (msg.match_elements<vec_type>()) {
      auto& xs = msg.get_as<vec_type>(0);
      downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
      for (auto& x : xs)
        fun_(state_, ds, x);
      return none;
    }
    return sec::unexpected_message;
  }

  message make_output_token(const stream_id& x) const override {
    return make_message(stream<output_type>{x});
  }

  optional<downstream_policy&> dp() override {
    return out_;
  }

  optional<upstream_policy&> up() override {
    return in_;
  }

  state_type& state() {
    return state_;
  }

  UpstreamPolicy& in() {
    return in_;
  }

  DownstreamPolicy& out() {
    return out_;
  }

private:
  state_type state_;
  Fun fun_;
  Cleanup cleanup_;
  UpstreamPolicy in_;
  DownstreamPolicy out_;
};

} // namespace caf

#endif // CAF_STREAM_STAGE_IMPL_HPP

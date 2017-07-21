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

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/downstream.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_stage_trait.hpp"

namespace caf {

template <class Fun, class Cleanup,
          class UpstreamPolicy, class DownstreamPolicy>
class stream_stage_impl : public stream_manager {
public:
  using trait = stream_stage_trait_t<Fun>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_stage_impl(local_actor* self, const stream_id&,
                    Fun fun, Cleanup cleanup)
      : fun_(std::move(fun)),
        cleanup_(std::move(cleanup)),
        in_(self),
        out_(self) {
    // nop
  }

  state_type& state() {
    return state_;
  }

  UpstreamPolicy& in() override {
    return in_;
  }

  DownstreamPolicy& out() override {
    return out_;
  }

  bool done() const override {
    return in_.closed() && out_.closed();
  }

protected:
  void input_closed(error reason) override {
    if (reason == none) {
      if (out_.buffered() == 0)
        out_.close();
    } else {
      out_.abort(std::move(reason));
    }
  }

  error process_batch(message& msg) override {
    CAF_LOG_TRACE(CAF_ARG(msg));
    using vec_type = std::vector<output_type>;
    if (msg.match_elements<vec_type>()) {
      auto& xs = msg.get_as<vec_type>(0);
      downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
      for (auto& x : xs)
        fun_(state_, ds, x);
      return none;
    }
    CAF_LOG_ERROR("received unexpected batch type");
    return sec::unexpected_message;
  }

  message make_output_token(const stream_id& x) const override {
    return make_message(stream<output_type>{x});
  }

  void downstream_demand(outbound_path* path, long) override {
    CAF_LOG_TRACE(CAF_ARG(path));
    auto hdl = path->hdl;
    if(out_.buffered() > 0)
      push();
    else if (in_.closed()) {
      // don't pass path->hdl: path can become invalid
      auto sid = path->sid;
      out_.remove_path(sid, hdl, none, false);
    }
    auto current_size = out_.buffered();
    auto desired_size = out_.credit();
    if (current_size < desired_size)
      in_.assign_credit(desired_size - current_size);
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

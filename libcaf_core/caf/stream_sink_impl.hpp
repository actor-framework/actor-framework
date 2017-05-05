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

#ifndef CAF_STREAM_SINK_IMPL_HPP
#define CAF_STREAM_SINK_IMPL_HPP

#include "caf/upstream.hpp"
#include "caf/stream_sink.hpp"
#include "caf/stream_sink_trait.hpp"

namespace caf {

template <class Fun, class Finalize>
class stream_sink_impl : public stream_sink {
public:
  using trait = stream_sink_trait_t<Fun, Finalize>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_sink_impl(local_actor* self, std::unique_ptr<upstream_policy> policy,
                   strong_actor_ptr&& orig_sender,
                   std::vector<strong_actor_ptr>&& svec, message_id mid,
                   Fun fun, Finalize fin)
      : stream_sink(&in_, std::move(orig_sender), std::move(svec), mid),
        fun_(std::move(fun)),
        fin_(std::move(fin)),
        in_(self, std::move(policy)) {
    // nop
  }

  expected<long> add_upstream(strong_actor_ptr& ptr, const stream_id& sid,
                              stream_priority prio) override {
    if (ptr)
      return in().add_path(ptr, sid, prio, min_buffer_size());
    return sec::invalid_argument;
  }

  error consume(message& msg) override {
    using vec_type = std::vector<input_type>;
    if (msg.match_elements<vec_type>()) {
      auto& xs = msg.get_as<vec_type>(0);
      for (auto& x : xs)
        fun_(state_, x);
      return none;
    }
    return sec::unexpected_message;
  }

  message finalize() override {
    return trait::make_result(state_, fin_);
  }

  optional<abstract_upstream&> get_upstream() override {
    return in_;
  }

  state_type& state() {
    return state_;
  }

private:
  state_type state_;
  Fun fun_;
  Finalize fin_;
  upstream<output_type> in_;
};

} // namespace caf

#endif // CAF_STREAM_SINK_IMPL_HPP

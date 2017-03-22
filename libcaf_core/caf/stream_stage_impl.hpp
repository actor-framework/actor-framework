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

#include "caf/upstream.hpp"
#include "caf/downstream.hpp"
#include "caf/stream_stage.hpp"
#include "caf/stream_stage_trait.hpp"

namespace caf {

template <class Fun, class Cleanup>
class stream_stage_impl : public stream_stage {
public:
  using trait = stream_stage_trait_t<Fun>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_stage_impl(local_actor* self,
                    const stream_id& sid,
                    typename abstract_upstream::policy_ptr in_policy,
                    typename abstract_downstream::policy_ptr out_policy,
                    Fun fun, Cleanup cleanup)
      : stream_stage(&in_, &out_),
        fun_(std::move(fun)),
        cleanup_(std::move(cleanup)),
        in_(self, std::move(in_policy)),
        out_(self, sid, std::move(out_policy)) {
    // nop
  }

  expected<size_t> add_upstream(strong_actor_ptr& ptr, const stream_id& sid,
                                stream_priority prio) final {
    if (ptr)
      return in().add_path(ptr, sid, prio, out_.buf_size(),
                           out_.available_credit());
    return sec::invalid_argument;
  }

  error process_batch(message& msg) final {
    using vec_type = std::vector<output_type>;
    if (msg.match_elements<vec_type>()) {
      auto& xs = msg.get_as<vec_type>(0);
      for (auto& x : xs)
        fun_(state_, out_, x);
      return none;
    }
    return sec::unexpected_message;
  }

  message make_output_token(const stream_id& x) const final {
    return make_message(stream<output_type>{x});
  }

  optional<abstract_downstream&> get_downstream() final {
    return out_;
  }

  optional<abstract_upstream&> get_upstream() final {
    return in_;
  }

  state_type& state() {
    return state_;
  }

private:
  state_type state_;
  Fun fun_;
  Cleanup cleanup_;
  upstream<output_type> in_;
  downstream<input_type> out_;
};

} // namespace caf

#endif // CAF_STREAM_STAGE_IMPL_HPP

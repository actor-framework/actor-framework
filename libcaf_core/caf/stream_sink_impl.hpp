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

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/message_id.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_sink_trait.hpp"

namespace caf {

template <class Fun, class Finalize, class Gatherer, class Scatterer>
class stream_sink_impl : public stream_manager {
public:
  using super = stream_manager;

  using trait = stream_sink_trait_t<Fun, Finalize>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_sink_impl(local_actor* self, Fun fun, Finalize fin)
      : fun_(std::move(fun)),
        fin_(std::move(fin)),
        in_(self) {
    // nop
  }

  state_type& state() {
    return state_;
  }

  Gatherer& in() override {
    return in_;
  }

  Scatterer& out() override {
    return out_;
  }

  bool done() const override {
    return in_.closed();
  }

protected:
  error process_batch(message& msg) override {
    CAF_LOG_TRACE(CAF_ARG(msg));
    using vec_type = std::vector<input_type>;
    if (msg.match_elements<vec_type>()) {
      auto& xs = msg.get_as<vec_type>(0);
      for (auto& x : xs)
        fun_(state_, x);
      return none;
    }
    CAF_LOG_ERROR("received unexpected batch type");
    return sec::unexpected_message;
  }

  message make_final_result() override {
    return trait::make_result(state_, fin_);
  }

private:
  state_type state_;
  Fun fun_;
  Finalize fin_;
  Gatherer in_;
  Scatterer out_;
};

} // namespace caf

#endif // CAF_STREAM_SINK_IMPL_HPP

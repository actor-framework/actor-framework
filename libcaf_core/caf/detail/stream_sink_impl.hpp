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

#ifndef CAF_DETAIL_STREAM_SINK_IMPL_HPP
#define CAF_DETAIL_STREAM_SINK_IMPL_HPP

#include "caf/config.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/message_id.hpp"
#include "caf/sec.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_sink_trait.hpp"
#include "caf/terminal_stream_scatterer.hpp"

#include "caf/policy/arg.hpp"

namespace caf {
namespace detail {

template <class Fun, class Finalize>
class stream_sink_impl : public stream_manager {
public:
  using super = stream_manager;

  using trait = stream_sink_trait_t<Fun, Finalize>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_sink_impl(local_actor* self, Fun fun, Finalize fin)
      : stream_manager(self),
        fun_(std::move(fun)),
        fin_(std::move(fin)),
        out_(self) {
    // nop
  }

  state_type& state() {
    return state_;
  }

  terminal_stream_scatterer& out() override {
    return out_;
  }

  bool done() const override {
    return this->inbound_paths_.empty();
  }

  error handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(msg));
    using vec_type = std::vector<input_type>;
    if (x.xs.match_elements<vec_type>()) {
      auto& xs = x.xs.get_as<vec_type>(0);
      for (auto& x : xs)
        fun_(state_, std::move(x));
      return none;
    }
    CAF_LOG_ERROR("received unexpected batch type");
    return sec::unexpected_message;
  }

protected:
  message make_final_result() override {
    return trait::make_result(state_, fin_);
  }

private:
  state_type state_;
  Fun fun_;
  Finalize fin_;
  terminal_stream_scatterer out_;
};

template <class Init, class Fun, class Finalize>
stream_manager_ptr make_stream_sink(local_actor* self, Init init, Fun f,
                                    Finalize fin) {
  using impl = stream_sink_impl<Fun, Finalize>;
  auto ptr = make_counted<impl>(self, std::move(f), std::move(fin));
  init(ptr->state());
  return ptr;
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_SINK_IMPL_HPP

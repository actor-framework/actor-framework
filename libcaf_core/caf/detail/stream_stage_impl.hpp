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

#ifndef CAF_DETAIL_STREAM_STAGE_IMPL_HPP
#define CAF_DETAIL_STREAM_STAGE_IMPL_HPP

#include "caf/downstream.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/outbound_path.hpp"
#include "caf/sec.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_stage_trait.hpp"

#include "caf/policy/arg.hpp"

namespace caf {
namespace detail {

template <class Fun, class Cleanup, class DownstreamPolicy>
class stream_stage_impl : public stream_manager {
public:
  using trait = stream_stage_trait_t<Fun>;

  using state_type = typename trait::state;

  using input_type = typename trait::input;

  using output_type = typename trait::output;

  stream_stage_impl(local_actor* self, Fun fun, Cleanup cleanup)
      : stream_manager(self),
        fun_(std::move(fun)),
        cleanup_(std::move(cleanup)),
        out_(self),
        open_inputs_(0) {
    // nop
  }

  state_type& state() {
    return state_;
  }

  DownstreamPolicy& out() override {
    return out_;
  }

  bool done() const override {
    return open_inputs_ == 0 && out_.clean();
  }

  void register_input_path(inbound_path* x) override {
    CAF_LOG_TRACE(CAF_ARG(*x));
    CAF_IGNORE_UNUSED(x);
    ++open_inputs_;
  }

  void deregister_input_path(inbound_path* x) noexcept override {
    CAF_LOG_TRACE(CAF_ARG(*x));
    CAF_IGNORE_UNUSED(x);
    --open_inputs_;
  }

  error handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(msg));
    using vec_type = std::vector<output_type>;
    if (x.xs.match_elements<vec_type>()) {
      auto& xs = x.xs.get_as<vec_type>(0);
      downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
      for (auto& x : xs)
        fun_(state_, ds, std::move(x));
      return none;
    }
    CAF_LOG_ERROR("received unexpected batch type");
    return sec::unexpected_message;
  }

  message make_output_token(const stream_id&) const override {
    // TODO: return make_message(stream<output_type>{x});
    return make_message();
  }

  bool congested() const override {
    return out_.buffered() >= 30;
  }

private:
  state_type state_;
  Fun fun_;
  Cleanup cleanup_;
  DownstreamPolicy out_;
  long open_inputs_;
};

template <class Init, class Fun, class Cleanup, class Scatterer>
stream_manager_ptr make_stream_stage(local_actor* self, Init init, Fun f,
                                     Cleanup cleanup, policy::arg<Scatterer>) {
  using impl = stream_stage_impl<Fun, Cleanup, Scatterer>;
  auto ptr = make_counted<impl>(self, std::move(f), std::move(cleanup));
  init(ptr->state());
  return ptr;
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_STAGE_IMPL_HPP

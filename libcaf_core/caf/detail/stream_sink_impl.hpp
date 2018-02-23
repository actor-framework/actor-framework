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

template <class Driver>
class stream_sink_impl : public stream_manager {
public:
  using super = stream_manager;

  using driver_type = Driver;

  using input_type = typename driver_type::input_type;

  template <class... Ts>
  stream_sink_impl(local_actor* self, Ts&&... xs)
      : stream_manager(self),
        driver_(std::forward<Ts>(xs)...),
        out_(self) {
    // nop
  }

  terminal_stream_scatterer& out() override {
    return out_;
  }

  bool done() const override {
    return !this->continuous() && this->inbound_paths_.empty();
  }

  void handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    using vec_type = std::vector<input_type>;
    if (x.xs.match_elements<vec_type>()) {
      auto& xs = x.xs.get_mutable_as<vec_type>(0);
      driver_.process(std::move(xs));
      return;
    }
    CAF_LOG_ERROR("received unexpected batch type (dropped)");
  }

protected:
  message make_final_result() override {
    return driver_.finalize();
  }

private:
  driver_type driver_;
  terminal_stream_scatterer out_;
};

template <class Driver, class... Ts>
stream_manager_ptr make_stream_sink(local_actor* self, Ts&&... xs) {
  using impl = stream_sink_impl<Driver>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_SINK_IMPL_HPP

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

#pragma once

#include "caf/config.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/message_id.hpp"
#include "caf/sec.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_sink.hpp"

#include "caf/policy/arg.hpp"

namespace caf {
namespace detail {

template <class Driver>
class stream_sink_impl : public Driver::sink_type {
public:
  using super = typename Driver::sink_type;

  using driver_type = Driver;

  using input_type = typename driver_type::input_type;

  template <class... Ts>
  stream_sink_impl(scheduled_actor* self, Ts&&... xs)
      : stream_manager(self),
        super(self),
        driver_(std::forward<Ts>(xs)...) {
    // nop
  }

  using super::handle;

  void handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    using vec_type = std::vector<input_type>;
    if (x.xs.match_elements<vec_type>()) {
      driver_.process(x.xs.get_mutable_as<vec_type>(0));
      return;
    }
    CAF_LOG_ERROR("received unexpected batch type (dropped)");
  }

  int32_t acquire_credit(inbound_path* path, int32_t desired) override {
    return driver_.acquire_credit(path, desired);
  }

  bool congested() const noexcept override {
    return driver_.congested();
  }

protected:
  void finalize(const error& reason) override {
    driver_.finalize(reason);
  }

  driver_type driver_;
};

template <class Driver, class... Ts>
typename Driver::sink_ptr_type make_stream_sink(scheduled_actor* self,
                                                Ts&&... xs) {
  using impl = stream_sink_impl<Driver>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace caf


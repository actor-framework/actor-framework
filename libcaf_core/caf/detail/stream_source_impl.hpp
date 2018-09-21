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

#include "caf/downstream.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/make_source_result.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {
namespace detail {

template <class Driver>
class stream_source_impl : public Driver::source_type {
public:
  // -- member types -----------------------------------------------------------

  using super = typename Driver::source_type;

  using driver_type = Driver;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  stream_source_impl(scheduled_actor* self, Ts&&... xs)
      : stream_manager(self),
        super(self),
        driver_(std::forward<Ts>(xs)...),
        at_end_(false) {
    // nop
  }

  // -- implementation of virtual functions ------------------------------------

  void shutdown() override {
    super::shutdown();
    at_end_ = true;
  }

  bool done() const override {
    return this->pending_handshakes_ == 0 && at_end_ && this->out_.clean();
  }

  bool generate_messages() override {
    CAF_LOG_TRACE("");
    if (at_end_)
      return false;
    auto hint = this->out_.capacity();
    CAF_LOG_DEBUG(CAF_ARG(hint));
    if (hint == 0)
      return false;
    downstream<typename Driver::output_type> ds{this->out_.buf()};
    driver_.pull(ds, hint);
    if (driver_.done())
      at_end_ = true;
    return hint != this->out_.capacity();
  }

protected:
  void finalize(const error& reason) override {
    driver_.finalize(reason);
  }

  Driver driver_;

private:
  bool at_end_;
};

template <class Driver, class... Ts>
typename Driver::source_ptr_type make_stream_source(scheduled_actor* self,
                                                    Ts&&... xs) {
  using impl = stream_source_impl<Driver>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace caf


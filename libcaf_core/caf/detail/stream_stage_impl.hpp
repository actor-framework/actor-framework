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
#include "caf/stream_stage.hpp"
#include "caf/stream_stage_trait.hpp"

namespace caf {
namespace detail {

template <class Driver, class Scatterer>
class stream_stage_impl : public Driver::stage_type {
public:
  // -- static asserts ---------------------------------------------------------

  static_assert(std::is_same<typename Driver::output_type,
                             typename Scatterer::value_type>::value,
                "Driver and Scatterer have incompatible types.");

  // -- member types -----------------------------------------------------------

  using super = typename Driver::stage_type;

  using driver_type = Driver;

  using scatterer_type = Scatterer;

  using input_type = typename driver_type::input_type;

  using output_type = typename driver_type::output_type;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  stream_stage_impl(local_actor* self, Ts&&... xs)
      : super(self),
        driver_(std::forward<Ts>(xs)...),
        out_(self) {
    // nop
  }

  // -- implementation of virtual functions ------------------------------------

  scatterer_type& out() override {
    return out_;
  }

  bool done() const override {
    return !this->continuous() && this->pending_handshakes_ == 0
           && this->inbound_paths_.empty() && out_.clean();
  }

  void handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    using vec_type = std::vector<input_type>;
    if (x.xs.match_elements<vec_type>()) {
      auto& xs = x.xs.get_mutable_as<vec_type>(0);
      downstream<output_type> ds{out_.buf()};
      driver_.process(std::move(xs), ds);
      return;
    }
    CAF_LOG_ERROR("received unexpected batch type (dropped)");
  }

  message make_handshake(stream_slot slot) const override {
    return make_message_from_tuple(driver_.make_handshake(slot));
  }

  bool congested() const noexcept override {
    return out_.capacity() == 0;
  }

protected:
  void finalize(const error& reason) override {
    driver_.finalize(reason);
  }

private:
  driver_type driver_;
  scatterer_type out_;
};

template <class Driver,
          class Scatterer = broadcast_scatterer<typename Driver::output_type>,
          class... Ts>
typename Driver::stage_ptr_type make_stream_stage(local_actor* self,
                                                  Ts&&... xs) {
  using impl = stream_stage_impl<Driver, Scatterer>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_STAGE_IMPL_HPP

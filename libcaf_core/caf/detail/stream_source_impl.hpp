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

#ifndef CAF_DETAIL_STREAM_SOURCE_IMPL_HPP
#define CAF_DETAIL_STREAM_SOURCE_IMPL_HPP

#include "caf/downstream.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {
namespace detail {

template <class Driver, class Scatterer>
class stream_source_impl : public Driver::source_type {
public:
  // -- static asserts ---------------------------------------------------------

  static_assert(std::is_same<typename Driver::output_type,
                             typename Scatterer::value_type>::value,
                "Driver and Scatterer have incompatible types.");

  // -- member types -----------------------------------------------------------

  using super = typename Driver::source_type;

  using driver_type = Driver;

  using scatterer_type = Scatterer;

  using output_type = typename driver_type::output_type;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  stream_source_impl(local_actor* self, Ts&&... xs)
      : super(self),
        at_end_(false),
        driver_(std::forward<Ts>(xs)...),
        out_(self) {
    // nop
  }

  // -- implementation of virtual functions ------------------------------------

  bool done() const override {
    return this->pending_handshakes_ == 0 && at_end_ && out_.clean();
  }

  Scatterer& out() override {
    return out_;
  }

  bool generate_messages() override {
    CAF_LOG_TRACE("");
    if (at_end_)
      return false;
    auto hint = out_.capacity();
    CAF_LOG_DEBUG(CAF_ARG(hint));
    if (hint == 0)
      return false;
    downstream<output_type> ds{out_.buf()};
    driver_.pull(ds, hint);
    if (driver_.done())
      at_end_ = true;
    return hint != out_.capacity();
  }

  message make_handshake() const override {
    return make_message_from_tuple(driver_.make_handshake());
  }

private:
  bool at_end_;
  Driver driver_;
  Scatterer out_;
};

template <class Driver,
          class Scatterer = broadcast_scatterer<typename Driver::output_type>,
          class... Ts>
typename Driver::source_ptr_type make_stream_source(local_actor* self,
                                                    Ts&&... xs) {
  using impl = stream_source_impl<Driver, Scatterer>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_SOURCE_IMPL_HPP

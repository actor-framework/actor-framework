// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/downstream.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/make_source_result.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf::detail {

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
    auto old_size = this->out_.buf().size();
    downstream<typename Driver::output_type> ds{this->out_.buf()};
    driver_.pull(ds, hint);
    if (driver_.done())
      at_end_ = true;
    auto new_size = this->out_.buf().size();
    this->out_.generated_messages(new_size - old_size);
    return new_size != old_size;
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
typename Driver::source_ptr_type
make_stream_source(scheduled_actor* self, Ts&&... xs) {
  using impl = stream_source_impl<Driver>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace caf::detail

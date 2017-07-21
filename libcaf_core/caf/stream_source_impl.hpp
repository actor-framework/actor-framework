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

#ifndef CAF_STREAM_SOURCE_IMPL_HPP
#define CAF_STREAM_SOURCE_IMPL_HPP

#include "caf/logger.hpp"
#include "caf/downstream.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_source_trait.hpp"
#include "caf/invalid_stream_gatherer.hpp"

namespace caf {

template <class Fun, class Predicate, class DownstreamPolicy>
class stream_source_impl : public stream_manager {
public:
  using trait = stream_source_trait_t<Fun>;

  using state_type = typename trait::state;

  using output_type = typename trait::output;

  stream_source_impl(local_actor* self, Fun fun, Predicate pred)
      : fun_(std::move(fun)),
        pred_(std::move(pred)),
        out_(self) {
    // nop
  }

  void generate(size_t num) {
    CAF_LOG_TRACE(CAF_ARG(num));
    downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
    fun_(state_, ds, num);
  }

  bool done() const override {
    return at_end() && out_.paths_clean();
  }

  invalid_stream_gatherer& in() override {
    return in_;
  }

  DownstreamPolicy& out() override {
    return out_;
  }

  state_type& state() {
    return state_;
  }

protected:
  bool at_end() const {
    return pred_(state_);
  }

  void downstream_demand(outbound_path* path, long) override {
    CAF_LOG_TRACE(CAF_ARG(path));
    if (!at_end()) {
      // produce new elements
      auto capacity = out_.credit();
      // TODO: fix issue where a source does not emit the last pieces if
      //       min_batch_size cannot be reached
      while (capacity >= out_.min_batch_size()) {
        auto current_size = out_.buffered();
        auto size_hint = std::min(capacity, out_.max_batch_size());
        if (size_hint > current_size)
          generate(static_cast<size_t>(size_hint - current_size));
        push();
        // Set available credit for next iteration.
        auto new_capacity = at_end() ? 0l : out_.credit();
        if (new_capacity != capacity)
          capacity = new_capacity;
      }
    } else if (out_.buffered() > 0) {
      push();
    } else {
      auto sid = path->sid;
      auto hdl = path->hdl;
      out().remove_path(sid, hdl, none, false);
    }
  }

private:
  state_type state_;
  Fun fun_;
  Predicate pred_;
  DownstreamPolicy out_;
  invalid_stream_gatherer in_;
};

} // namespace caf

#endif // CAF_STREAM_SOURCE_IMPL_HPP

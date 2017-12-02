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

#ifndef CAF_STREAM_SOURCE_IMPL_HPP
#define CAF_STREAM_SOURCE_IMPL_HPP

#include "caf/logger.hpp"
#include "caf/downstream.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_source_trait.hpp"

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

  bool done() const override {
    return at_end() && out_.paths_clean();
  }


  DownstreamPolicy& out() override {
    return out_;
  }

  bool generate_messages() override {
    // Produce new elements. If we have less elements buffered (bsize) than fit
    // into a single batch (dbs = desired batch size) then we fill up the
    // buffer up to that point. Otherwise, we fill up the buffer up to its
    // capacity since we are currently waiting for downstream demand and can
    // use the delay to store batches upfront.
    size_t size_hint;
    auto bsize = out_.buffered();
    auto dbs  = out_.desired_batch_size();
    if (bsize < dbs) {
      size_hint= static_cast<size_t>(dbs - bsize);
    } else {
      auto bmax = out_.min_buffer_size();
      if (bsize < bmax)
        size_hint = static_cast<size_t>(bmax - bsize);
      else
        return false;
    }
    downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
    fun_(state_, ds, size_hint);
    return out_.buffered() != bsize;
  }

  state_type& state() {
    return state_;
  }

protected:
  bool at_end() const {
    return pred_(state_);
  }

  void downstream_demand(outbound_path*, long) override {
    // TODO: implment me
  }

private:
  state_type state_;
  Fun fun_;
  Predicate pred_;
  DownstreamPolicy out_;
};

} // namespace caf

#endif // CAF_STREAM_SOURCE_IMPL_HPP

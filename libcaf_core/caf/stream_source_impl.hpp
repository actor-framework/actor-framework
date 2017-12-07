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

#include "caf/downstream.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_source_trait.hpp"

#include "caf/policy/arg.hpp"

namespace caf {

template <class Fun, class Predicate, class DownstreamPolicy>
class stream_source_impl : public stream_manager {
public:
  using trait = stream_source_trait_t<Fun>;

  using state_type = typename trait::state;

  using output_type = typename trait::output;

  stream_source_impl(local_actor* self, Fun fun, Predicate pred)
      : stream_manager(self),
        at_end_(false),
        fun_(std::move(fun)),
        pred_(std::move(pred)),
        out_(self) {
    // nop
  }

  bool done() const override {
    return at_end_ && out_.clean();
  }


  DownstreamPolicy& out() override {
    return out_;
  }


  bool generate_messages() override {
    if (at_end_)
      return false;
    auto hint = out_.capacity();
    if (hint == 0)
      return false;
    downstream<typename DownstreamPolicy::value_type> ds{out_.buf()};
    fun_(state_, ds, hint);
    if (pred_(state_))
      at_end_ = true;
    return hint != out_.capacity();
  }

  state_type& state() {
    return state_;
  }

  bool at_end() const noexcept {
    return at_end_;;
  }

private:
  bool at_end_;
  state_type state_;
  Fun fun_;
  Predicate pred_;
  DownstreamPolicy out_;
};

template <class Init, class Fun, class Predicate, class Scatterer>
stream_manager_ptr make_stream_source(local_actor* self, Init init, Fun f,
                                      Predicate p, policy::arg<Scatterer>) {
  using impl = stream_source_impl < Fun, Predicate, Scatterer>;
  auto ptr = make_counted<impl>(self, std::move(f), std::move(p));
  init(ptr->state());
  return ptr;
}

} // namespace caf

#endif // CAF_STREAM_SOURCE_IMPL_HPP

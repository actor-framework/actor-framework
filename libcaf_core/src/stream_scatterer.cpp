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

#include "caf/stream_scatterer.hpp"

#include <functional>

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

namespace {

using pointer = stream_scatterer::path_ptr;

using unique_pointer = stream_scatterer::path_unique_ptr;

} // namespace <anonymous>

stream_scatterer::stream_scatterer(local_actor* self) : self_(self) {
  // nop
}

stream_scatterer::~stream_scatterer() {
  // nop
}

pointer stream_scatterer::add_path(stream_slots slots,
                                   strong_actor_ptr target) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(target) << CAF_ARG(handshake_data));
  auto res = paths_.emplace(slots, nullptr);
  if (res.second) {
    auto ptr = new outbound_path(slots, std::move(target));
    res.first->second.reset(ptr);
    return ptr;
  }
  return nullptr;
}

unique_pointer stream_scatterer::take_path(stream_slots slots) noexcept {
  unique_pointer result;
  auto i = paths_.find(slots);
  if (i != paths_.end()) {
    result.swap(i->second);
    paths_.erase(i);
  }
  return result;
}

pointer stream_scatterer::path(stream_slots slots) noexcept {
  auto i = paths_.find(slots);
  return i != paths_.end() ? i->second.get() : nullptr;
}

pointer stream_scatterer::path_at(size_t index) noexcept {
  CAF_ASSERT(paths_.size() < index);
  return (paths_.container())[index].second.get();
}

unique_pointer stream_scatterer::take_path_at(size_t index) noexcept {
  CAF_ASSERT(paths_.size() < index);
  unique_pointer result;
  auto i = paths_.begin() + index;
  result.swap(i->second);
  paths_.erase(i);
  return result;
}

bool stream_scatterer::clean() const noexcept {
  auto pred = [](const map_type::value_type& kvp) {
    auto& p = *kvp.second;
    return p.next_batch_id == p.next_ack_id
           && p.unacknowledged_batches.empty();
  };
  return buffered() == 0 && std::all_of(paths_.begin(), paths_.end(), pred);
}

void stream_scatterer::close() {
  for (auto i = paths_.begin(); i != paths_.end(); ++i) {
    about_to_erase(i, false, nullptr);
    i->second->emit_regular_shutdown(self_);
  }
  paths_.clear();
}

void stream_scatterer::abort(error reason) {
  auto& vec = paths_.container();
  if (vec.empty())
    return;
  auto i = paths_.begin();
  auto s = paths_.end() - 1;
  for (; i != s; ++i) {
    auto tmp = reason;
    about_to_erase(i, false, &tmp);
  }
  about_to_erase(i, false, &reason);
  vec.clear();
}

namespace {

template <class BinOp>
struct accumulate_helper {
  using value_type = stream_scatterer::map_type::value_type;
  BinOp op;
  size_t operator()(size_t x, const value_type& y) const noexcept {
    return op(x, y.second->open_credit);
  }
  size_t operator()(const value_type& x, size_t y) const noexcept {
    return op(x.second->open_credit, y);
  }
};

template <class BinOp>
struct accumulate_helper<BinOp> make_accumulate_helper(BinOp f) {
  return {f};
}

template <class F, class T, class Container>
T fold(F fun, T init, const Container& xs) {
  return std::accumulate(xs.begin(), xs.end(), std::move(init),
                         make_accumulate_helper(std::move(fun)));
}

using bin_op_fptr = const size_t& (*)(const size_t&, const size_t&);

} // namespace <anonymous>

size_t stream_scatterer::min_credit() const noexcept {
  if (empty())
    return 0u;
  return fold(static_cast<bin_op_fptr>(&std::min<size_t>),
              std::numeric_limits<size_t>::max(), paths_);
}

size_t stream_scatterer::max_credit() const noexcept {
  return fold(static_cast<bin_op_fptr>(&std::max<size_t>),
              size_t{0u}, paths_);
}

size_t stream_scatterer::total_credit() const noexcept {
  return fold(std::plus<size_t>{}, size_t{0u}, paths_);
}

void stream_scatterer::about_to_erase(map_type::iterator i, bool silent,
                                      error* reason) {
  if (!silent) {
    if (reason == nullptr)
      i->second->emit_regular_shutdown(self_);
    else
      i->second->emit_irregular_shutdown(self_, std::move(*reason));
  }
}

} // namespace caf

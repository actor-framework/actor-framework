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

#ifndef CAF_FUSED_SCATTERER
#define CAF_FUSED_SCATTERER

#include <tuple>
#include <cstddef>

#include "caf/logger.hpp"
#include "caf/stream_scatterer.hpp"

namespace caf {

namespace detail {

/// Utility function for repeating `x` for a given template parameter pack.
template <class T, class U>
U pack_repeat(U x) {
  return x;
}

template <class Iter>
class ptr_array_initializer {
public:
  ptr_array_initializer(Iter first) : i_(first) {
    // nop
  }

  void operator()() {
    // end of recursion
  }

  template <class T, class... Ts>
  void operator()(T& x, Ts&... xs) {
    *i_ = &x;
    ++i_;
    (*this)(xs...);
  }

private:
  Iter i_;
};

struct scatterer_selector {
  inline stream_scatterer* operator()(const message&) {
    return nullptr;
  }

  template <class T, class... Ts>
  stream_scatterer* operator()(const message& msg, T& x, Ts&... xs) {
    if (msg.match_element<stream<typename T::value_type>>(0))
      return &x;
    return (*this)(msg, xs...);
  }
};

} // namespace detail

/// A scatterer that delegates to any number of sub-scatterers. Data is only
/// pushed to the main scatterer `T` per default.
template <class T, class... Ts>
class fused_scatterer : public stream_scatterer {
public:
  using substreams_tuple = std::tuple<T, Ts...>;

  using pointer = stream_scatterer*;
  using const_pointer = const pointer;

  using iterator = pointer*;
  using const_iterator = const pointer*;

  fused_scatterer(local_actor* self)
      : substreams_(self, detail::pack_repeat<Ts>(self)...) {
    detail::ptr_array_initializer<pointer*> f{ptrs_};
    auto indices = detail::get_indices(substreams_);
    detail::apply_args(f, indices, substreams_);
  }

  path_ptr add_path(const stream_id& sid, strong_actor_ptr origin,
                    strong_actor_ptr sink_ptr,
                    mailbox_element::forwarding_stack stages,
                    message_id handshake_mid, message handshake_data,
                    stream_priority prio, bool redeployable) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(origin) << CAF_ARG(sink_ptr)
                  << CAF_ARG(stages) << CAF_ARG(handshake_mid)
                  << CAF_ARG(handshake_data) << CAF_ARG(prio)
                  << CAF_ARG(redeployable));
    auto ptr = substream_by_handshake_type(handshake_data);
    if (!ptr)
      return nullptr;
    return ptr->add_path(sid, std::move(origin), std::move(sink_ptr),
                         std::move(stages), handshake_mid,
                         std::move(handshake_data), prio, redeployable);
  }

  path_ptr confirm_path(const stream_id& sid, const actor_addr& from,
                        strong_actor_ptr to, long initial_demand,
                        bool redeployable) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(from) << CAF_ARG(to)
                  << CAF_ARG(initial_demand) << CAF_ARG(redeployable));
    return first_hit([&](pointer ptr) -> outbound_path* {
      // Note: we cannot blindly try `confirm_path` on each scatterer, because
      // this will trigger forced_close messages.
      if (ptr->find(sid, from) == nullptr)
        return nullptr;
      return ptr->confirm_path(sid, from, to, initial_demand, redeployable);
    });
  }

  bool remove_path(const stream_id& sid, const actor_addr& addr, error reason,
                   bool silent) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(addr) << CAF_ARG(reason)
                  << CAF_ARG(silent));
    return std::any_of(begin(), end(), [&](pointer x) {
      return x->remove_path(sid, addr, reason, silent);
    });
  }

  bool paths_clean() const override {
    return std::all_of(begin(), end(),
                       [&](const_pointer x) { return x->paths_clean(); });
  }

  void close() override {
    CAF_LOG_TRACE("");
    for (auto ptr : ptrs_)
      ptr->close();
  }

  void abort(error reason) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    for (auto ptr : ptrs_)
      ptr->abort(reason);
  }

  long num_paths() const override {
    return std::accumulate(begin(), end(), 0l, [](long x, const_pointer y) {
      return x + y->num_paths();
    });
  }

  bool closed() const override {
    return std::all_of(begin(), end(),
                       [&](const_pointer x) { return x->closed(); });
  }

  bool continuous() const override {
    return std::any_of(begin(), end(),
                       [&](const_pointer x) { return x->continuous(); });
  }

  void continuous(bool value) override {
    for (auto ptr : ptrs_)
      ptr->continuous(value);
  }

  void emit_batches() override {
    CAF_LOG_TRACE("");
    for (auto ptr : ptrs_)
      ptr->emit_batches();
  }

  path_ptr find(const stream_id& sid, const actor_addr& x) override{
    return first_hit([&](const_pointer ptr) { return ptr->find(sid, x); });
  }

  path_ptr path_at(size_t idx) override {
    auto i = std::begin(ptrs_);
    auto e = std::end(ptrs_);
    while (i != e) {
      auto np = static_cast<size_t>((*i)->num_paths());
      if (idx < np)
        return (*i)->path_at(idx);
      idx -= np;
    }
    return nullptr;
  }

  long credit() const override {
    return std::accumulate(
      begin(), end(), std::numeric_limits<long>::max(),
      [](long x, pointer y) { return std::min(x, y->credit()); });
  }

  long buffered() const override {
    return std::accumulate(begin(), end(), 0l, [](long x, const_pointer y) {
      return x + y->buffered();
    });
  }

  long min_batch_size() const override {
    return main_stream().min_batch_size();
  }

  long max_batch_size() const override {
    return main_stream().max_batch_size();
  }

  long min_buffer_size() const override {
    return main_stream().min_buffer_size();
  }

  duration max_batch_delay() const override {
    return main_stream().max_batch_delay();
  }

  void min_batch_size(long x) override {
    main_stream().min_batch_size(x);
  }

  void max_batch_size(long x) override {
    main_stream().max_batch_size(x);
  }

  void min_buffer_size(long x) override {
    main_stream().min_buffer_size(x);
  }

  void max_batch_delay(duration x) override {
    main_stream().max_batch_delay(x);
  }

  template <class... Us>
  void push(Us&&... xs) {
    main_stream().push(std::forward<Us>(xs)...);
  }

  iterator begin() {
    return std::begin(ptrs_);
  }

  const_iterator begin() const {
    return std::begin(ptrs_);
  }

  iterator end() {
    return std::end(ptrs_);
  }

  const_iterator end() const {
    return std::end(ptrs_);
  }

  T& main_stream() {
    return std::get<0>(substreams_);
  }

  const T& main_stream() const {
    return std::get<0>(substreams_);
  }

  template <size_t I>
  typename std::tuple_element<I, substreams_tuple>::type& substream() {
    return std::get<I>(substreams_);
  }

  template <size_t I>
  const typename std::tuple_element<I, substreams_tuple>::type&
  substream() const {
    return std::get<I>(substreams_);
  }

  stream_scatterer* substream_by_handshake_type(const message& msg) {
    detail::scatterer_selector f;
    auto indices = detail::get_indices(substreams_);
    return detail::apply_args_prefixed(f, indices, substreams_, msg);
  }

private:
  template <class F>
  path_ptr first_hit(F f) {
    for (auto ptr : ptrs_) {
      auto result = f(ptr);
      if (result != nullptr)
        return result;
    }
    return nullptr;
  }

  substreams_tuple substreams_;
  stream_scatterer* ptrs_[sizeof...(Ts) + 1];
};

} // namespace caf

#endif // CAF_FUSED_SCATTERER


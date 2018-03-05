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

#ifndef CAF_FUSED_SCATTERER
#define CAF_FUSED_SCATTERER

#include <tuple>
#include <cstddef>

#include "caf/logger.hpp"
#include "caf/stream_scatterer.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/unordered_flat_map.hpp"

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

template <size_t I, size_t E>
struct init_ptr_array {
  template <class... Ts>
  static void apply(stream_scatterer* (&xs)[E], std::tuple<Ts...>& ys)  {
    xs[I] = &std::get<I>(ys);
    init_ptr_array<I + 1, E>::apply(xs, ys);
  }
};

template <size_t I>
struct init_ptr_array<I, I> {
  template <class... Ts>
  static void apply(stream_scatterer* (&)[I], std::tuple<Ts...>&)  {
    // nop
  }
};


} // namespace detail

/// A scatterer that delegates to any number of sub-scatterers. Data is only
/// pushed to the main scatterer `T` per default.
template <class T, class... Ts>
class fused_scatterer : public stream_scatterer {
public:
  // -- member and nested types ------------------------------------------------

  /// Base type.
  using super = stream_scatterer;

  /// A tuple holding all nested scatterers.
  using nested_scatterers = std::tuple<T, Ts...>;

  /// State held for each slot.
  struct non_owning_ptr {
    outbound_path* ptr;
    stream_scatterer* owner;
  };

  /// Maps slots to path and nested scatterer.
  using map_type = detail::unordered_flat_map<stream_slot, non_owning_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  fused_scatterer(scheduled_actor* self)
      : super(self),
        nested_(self, detail::pack_repeat<Ts...>(self)) {
    detail::init_ptr_array<0, sizeof...(Ts) + 1>::apply(ptrs_, nested_);
  }

  // -- properties -------------------------------------------------------------

  template <class U>
  U& get() {
    static constexpr size_t i =
      detail::tl_index_of<detail::type_list<T, Ts...>, U>::value;
    return std::get<i>(nested_);
    // TODO: replace with this line when switching to C++14
    // return std::get<U>(substreams_);
  }

  template <class U>
  void assign(stream_slot slot) {
    paths_.emplace(slot, non_owning_ptr{nullptr, &get<U>()});
  }

  // -- overridden functions ---------------------------------------------------

  size_t num_paths() const noexcept override {
    return paths_.size();
  }

  /// Requires a previous call to `assign<T>(slot.sender)`.
  path_ptr add_path(stream_slots slots, strong_actor_ptr target) override {
    CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(target));
    auto e = paths_.end();
    auto i = paths_.find(slots.sender);
    if (i == e) {
      CAF_LOG_ERROR("no scatterer assigned:" << CAF_ARG(slots.sender));
      return nullptr;
    }
    if (i->second.ptr != nullptr) {
      CAF_LOG_ERROR("multiple calls to add_path:" << CAF_ARG(slots.sender));
      return nullptr;
    }
    auto result = i->second.owner->add_path(slots, target);
    if (result == nullptr) {
      CAF_LOG_ERROR("nested scatterer unable to add path:"
                    << CAF_ARG(slots.sender));
      paths_.erase(i);
      return nullptr;
    }
    i->second.ptr = result;
    return result;
  }

  bool remove_path(stream_slot slot, error reason,
                   bool silent) noexcept override {
    auto i = paths_.find(slot);
    if (i == paths_.end())
      return false;
    auto owner = i->second.owner;
    paths_.erase(i);
    return owner->remove_path(slot, std::move(reason), silent);
  }

  path_ptr path(stream_slot slot) noexcept override {
    auto i = paths_.find(slot);
    if (i == paths_.end())
      return nullptr;
    return i->second.ptr;
  }

  void close() override {
    CAF_LOG_TRACE(CAF_ARG(paths_));
    for (auto ptr : ptrs_)
      ptr->close();
    paths_.clear();
  }

  void abort(error reason) override {
    CAF_LOG_TRACE(CAF_ARG(paths_));
    for (auto ptr : ptrs_)
      ptr->abort(reason);
    paths_.clear();
  }

  void emit_batches() override {
    CAF_LOG_TRACE("");
    for (auto ptr : ptrs_)
      ptr->emit_batches();
  }

  void force_emit_batches() override {
    CAF_LOG_TRACE("");
    for (auto ptr : ptrs_)
      ptr->force_emit_batches();
  }

  size_t capacity() const noexcept override {
    // Get the minimum of all available capacities.
    size_t result = std::numeric_limits<size_t>::max();
    for (auto ptr : ptrs_)
      result = std::min(result, ptr->capacity());
    return result;
  }

  size_t buffered() const noexcept override {
    // Get the maximum of all available buffer sizes.
    size_t result = 0;
    for (auto ptr : ptrs_)
      result = std::max(result, ptr->buffered());
    return result;
  }

  message make_handshake_token(stream_slot slot) const override {
    auto i = paths_.find(slot);
    if (i != paths_.end())
      return i->second.owner->make_handshake_token(slot);
    CAF_LOG_ERROR("no scatterer available:" << CAF_ARG(slot));
    return make_message(stream<message>{slot});
  }

  void clear_paths() override {
    for (auto ptr : ptrs_)
      ptr->clear_paths();
    paths_.clear();
  }

protected:
  void for_each_path_impl(path_visitor& f) override {
    for (auto& kvp : paths_)
      f(*kvp.second.ptr);
  }

  bool check_paths_impl(path_algorithm algo,
                        path_predicate& pred) const noexcept override {
    auto f = [&](const typename map_type::value_type& x) {
      return pred(*x.second.ptr);
    };
    switch (algo) {
      default: // all_of
        return std::all_of(paths_.begin(), paths_.end(), f);
      case path_algorithm::any_of:
        return std::any_of(paths_.begin(), paths_.end(), f);
      case path_algorithm::none_of:
        return std::none_of(paths_.begin(), paths_.end(), f);
    }
  }

private:
  nested_scatterers nested_;
  stream_scatterer* ptrs_[sizeof...(Ts) + 1];
  map_type paths_;
};

} // namespace caf

#endif // CAF_FUSED_SCATTERER


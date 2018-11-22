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

#include <algorithm>

#include "caf/buffered_downstream_manager.hpp"
#include "caf/outbound_path.hpp"
#include "caf/raise_error.hpp"

#include "caf/detail/algorithms.hpp"
#include "caf/detail/path_state.hpp"
#include "caf/detail/select_all.hpp"
#include "caf/detail/unordered_flat_map.hpp"

namespace caf {

template <class T, class Filter = unit_t, class Select = detail::select_all>
class broadcast_downstream_manager : public buffered_downstream_manager<T> {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = buffered_downstream_manager<T>;

  /// Type of `paths_`.
  using typename super::map_type;

  /// Unique pointer to an outbound path.
  using typename super::unique_path_ptr;

  /// Enables or disables output per path.
  using filter_type = Filter;

  /// Function object for evaluating filters.
  using select_type = Select;

  /// Container for caching `T`s per path with active filter.
  using path_state = detail::path_state<Filter, T>;

  /// Maps slot IDs to caches.
  using state_map_type = detail::unordered_flat_map<stream_slot, path_state>;

  // -- constructors, destructors, and assignment operators --------------------

  broadcast_downstream_manager(stream_manager* parent) : super(parent) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  size_t buffered() const noexcept override {
    // We have a central buffer, but also an additional buffer at each path. We
    // return the maximum size to reflect the current worst case.
    size_t central_buf = this->buf_.size();
    size_t max_path_buf = 0;
    for (auto& kvp : state_map_)
      max_path_buf = std::max(max_path_buf, kvp.second.buf.size());
    return central_buf + max_path_buf;
  }

  size_t buffered(stream_slot slot) const noexcept override {
    auto i = state_map_.find(slot);
    return this->buf_.size()
           + (i != state_map_.end() ? i->second.buf.size() : 0u);
  }

  int32_t max_capacity() const noexcept override {
    // The maximum capacity is limited by the slowest downstream path.
    auto result = std::numeric_limits<int32_t>::max();
    for (auto& kvp : this->paths_) {
      auto mc = kvp.second->max_capacity;
      // max_capacity is 0 if and only if we didn't receive an ack_batch yet.
      if (mc > 0)
        result = std::min(result, mc);
    }
    return result;
  }

  /// Sets the filter for `slot` to `filter`. Inserts a new element if `slot`
  /// is a new path.
  void set_filter(stream_slot slot, filter_type new_filter) {
    CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(new_filter));
    filter(slot) = std::move(new_filter);
  }

  /// Returns the filter for `slot`. Inserts a new element if `slot` is a new
  /// path.
  filter_type& filter(stream_slot slot) {
    auto i = state_map_.find(slot);
    if (i != state_map_.end())
      return i->second.filter;
    CAF_RAISE_ERROR("invalid slot");
  }

  /// Returns whether all filters satisfy the predicate as if applying
  /// `std::all_of`.
  template <class UnaryPredicate>
  bool all_filters(UnaryPredicate predicate) {
    return std::all_of(state_map_.begin(), state_map_.end(),
                       [&](const typename state_map_type::value_type& kvp) {
                         return predicate(kvp.second.filter);
                       });
  }

  /// Returns whether any filter satisfies the predicate as if applying
  /// `std::any_of`.
  template <class UnaryPredicate>
  bool any_filter(UnaryPredicate predicate) {
    return std::any_of(state_map_.begin(), state_map_.end(),
                       [&](const typename state_map_type::value_type& kvp) {
                         return predicate(kvp.second.filter);
                       });
  }

  /// Returns whether no filter satisfies the predicate as if applying
  /// `std::none_of`.
  template <class UnaryPredicate>
  bool no_filter(UnaryPredicate predicate) {
    return std::none_of(state_map_.begin(), state_map_.end(),
                        [&](const typename state_map_type::value_type& kvp) {
                          return predicate(kvp.second.filter);
                        });
  }

  /// Returns the broadcast states for all paths.
  state_map_type& states() {
    return state_map_;
  }

  /// Returns the broadcast states for all paths.
  const state_map_type& states() const {
    return state_map_;
  }

  /// Returns the selector for filtering outgoing data.
  select_type& selector() {
    return select_;
  }

  /// Returns the selector for filtering outgoing data.
  const select_type& selector() const {
    return select_;
  }

  // -- overridden functions ---------------------------------------------------

  bool insert_path(unique_path_ptr ptr) override {
    CAF_LOG_TRACE(CAF_ARG(ptr));
    // Make sure state_map_ and paths_ are always equally sorted, otherwise
    // we'll run into UB when calling `zip_foreach`.
    CAF_ASSERT(state_map_.size() == this->paths_.size());
    auto slot = ptr->slots.sender;
    // Append to the regular path map.
    if (!super::insert_path(std::move(ptr))) {
      CAF_LOG_DEBUG("unable to insert path at slot" << slot);
      return false;
    }
    // Append to the state map.
    if (!state_map_.emplace(slot, path_state{}).second) {
      CAF_LOG_DEBUG("unable to add state for slot" << slot);
      super::remove_path(slot, none, true);
      return false;
    }
    return true;
  }

  void emit_batches() override {
    CAF_LOG_TRACE(CAF_ARG2("buffered", this->buffered())
                  << CAF_ARG2("paths", this->paths_.size()));
    emit_batches_impl(false);
  }

  void force_emit_batches() override {
    CAF_LOG_TRACE(CAF_ARG2("buffered", this->buffered())
                  << CAF_ARG2("paths", this->paths_.size()));
    emit_batches_impl(true);
  }

  /// Forces the manager flush its buffer to the individual path buffers.
  void fan_out_flush() {
    auto& buf = this->buf_;
    auto f = [&](typename map_type::value_type& x,
                 typename state_map_type::value_type& y) {
      // Don't push new data into a closing path.
      if (x.second->closing)
        return;
      // Push data from the global buffer to path buffers.
      auto& st = y.second;
      // TODO: replace with `if constexpr` when switching to C++17
      if (std::is_same<select_type, detail::select_all>::value) {
        st.buf.insert(st.buf.end(), buf.begin(), buf.end());
      } else {
        for (auto& piece : buf)
          if (select_(st.filter, piece))
            st.buf.emplace_back(piece);
      }
    };
    detail::zip_foreach(f, this->paths_.container(), state_map_.container());
    buf.clear();
  }

protected:
  void about_to_erase(outbound_path* ptr, bool silent,
                      error* reason) override {
    CAF_ASSERT(ptr != nullptr);
    CAF_LOG_TRACE(CAF_ARG2("slot", ptr->slots.sender) << CAF_ARG(silent) <<
                  CAF_ARG(reason));
    state_map_.erase(ptr->slots.sender);
    super::about_to_erase(ptr, silent, reason);
  }

private:
  void emit_batches_impl(bool force_underfull) {
    CAF_ASSERT(this->paths_.size() <= state_map_.size());
    if (this->paths_.empty())
      return;
    // Calculate the chunk size, i.e., how many more items we can put to our
    // caches at the most.
    auto not_closing = [&](typename map_type::value_type& x,
                           typename state_map_type::value_type&) {
      return !x.second->closing;
    };
    // Returns the minimum of `interim` and `get_credit(x) - get_cache_size(y)`.
    auto f = [&](size_t interim, typename map_type::value_type& x,
                 typename state_map_type::value_type& y) {
      auto credit = static_cast<size_t>(x.second->open_credit);
      auto cache_size = y.second.buf.size();
      return std::min(interim, credit > cache_size ? credit - cache_size : 0u);
    };
    auto chunk_size = detail::zip_fold_if(f, not_closing,
                                          std::numeric_limits<size_t>::max(),
                                          this->paths_.container(),
                                          state_map_.container());
    if (chunk_size == std::numeric_limits<size_t>::max()) {
      // All paths are closing, simply try forcing out more data and return.
      auto g = [&](typename map_type::value_type& x,
                   typename state_map_type::value_type& y) {
        // Always force batches on closing paths.
        x.second->emit_batches(this->self(), y.second.buf, true);
      };
      detail::zip_foreach(g, this->paths_.container(), state_map_.container());
      return;
    }

    auto chunk = this->get_chunk(chunk_size);
    if (chunk.empty()) {
      auto g = [&](typename map_type::value_type& x,
                   typename state_map_type::value_type& y) {
        // Always force batches on closing paths.
        x.second->emit_batches(this->self(), y.second.buf,
                               force_underfull || x.second->closing);
      };
      detail::zip_foreach(g, this->paths_.container(), state_map_.container());
    } else {
      auto g = [&](typename map_type::value_type& x,
                   typename state_map_type::value_type& y) {
        auto& st = y.second;
        // Don't enqueue new data into a closing path.
        if (!x.second->closing) {
          // Push data from the global buffer to path buffers.
          // TODO: replace with `if constexpr` when switching to C++17
          if (std::is_same<select_type, detail::select_all>::value) {
            st.buf.insert(st.buf.end(), chunk.begin(), chunk.end());
          } else {
            for (auto& piece : chunk)
              if (select_(st.filter, piece))
                st.buf.emplace_back(piece);
          }
        }
        // Always force batches on closing paths.
        x.second->emit_batches(this->self(), st.buf,
                               force_underfull || x.second->closing);
      };
      detail::zip_foreach(g, this->paths_.container(), state_map_.container());
    }
  }

  state_map_type state_map_;
  select_type select_;
};

} // namespace caf


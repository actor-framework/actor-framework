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

#ifndef CAF_BROADCAST_SCATTERER_HPP
#define CAF_BROADCAST_SCATTERER_HPP

#include "caf/buffered_scatterer.hpp"
#include "caf/outbound_path.hpp"

#include "caf/detail/algorithms.hpp"
#include "caf/detail/path_state.hpp"
#include "caf/detail/select_all.hpp"

namespace caf {

template <class T, class Filter = unit_t, class Select = detail::select_all>
class broadcast_scatterer : public buffered_scatterer<T> {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = buffered_scatterer<T>;

  /// Type of `paths_`.
  using typename super::map_type;

  /// Enables or disables output per path.
  using filter_type = Filter;

  /// Function object for evaluating filters.
  using select_type = Select;

  /// Container for caching `T`s per path with active filter.
  using path_state = detail::path_state<Filter, T>;

  /// Maps slot IDs to caches.
  using state_map_type = detail::unordered_flat_map<stream_slot, path_state>;

  // -- constructors, destructors, and assignment operators --------------------

  broadcast_scatterer(local_actor* selfptr) : super(selfptr) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Sets the filter for `x` to `f` and inserts `x` into the appropriate lane.
  /// @pre `x` is not registered on *any* lane
  void set_filter(stream_slot slot, filter_type filter) {
    CAF_LOG_TRACE(CAF_ARG(slot) << CAF_ARG(filter));
    state_map_[slot].filter = std::move(filter);
  }

  // -- overridden functions ---------------------------------------------------

  typename super::path_ptr add_path(stream_slots slots,
                                    strong_actor_ptr target) override {
    state_map_.emplace(slots.sender, path_state{});
    return super::add_path(slots, target);
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

protected:
  void about_to_erase(typename super::map_type::iterator i, bool silent,
                      error* reason) override {
    CAF_LOG_DEBUG("remove cache:" << CAF_ARG2("slot", i->second->slots.sender));
    state_map_.erase(i->second->slots.sender);
    super::about_to_erase(i, silent, reason);
  }

private:
  void emit_batches_impl(bool force_underfull) {
    CAF_ASSERT(this->paths_.size() == state_map_.size());
    if (this->paths_.empty())
      return;
    // Calculate the chunk size, i.e., how many more items we can put to our
    // caches at the most.
    struct get_credit {
      inline size_t operator()(typename map_type::value_type& x) {
        return static_cast<size_t>(x.second->open_credit);
      }
    };
    struct get_cache_size {
      inline size_t operator()(typename state_map_type::value_type& x) {
        return x.second.buf.size();
      }
    };
    auto f = [](size_t x, size_t credit, size_t cache_size) {
      auto y = credit > cache_size ? credit - cache_size : 0u;
      return x < y ? x : y;
    };
    auto chunk_size = detail::zip_fold(
      f, std::numeric_limits<size_t>::max(),
      detail::make_container_view<get_credit>(this->paths_.container()),
      detail::make_container_view<get_cache_size>(state_map_.container()));
    auto chunk = this->get_chunk(chunk_size);
    if (chunk.empty()) {
      auto g = [&](typename map_type::value_type& x,
                   typename state_map_type::value_type& y) {
        x.second->emit_batches(this->self_, y.second.buf, force_underfull);
      };
      detail::zip_foreach(g, this->paths_.container(), state_map_.container());
    } else {
      auto g = [&](typename map_type::value_type& x,
                   typename state_map_type::value_type& y) {
        // TODO: replace with `if constexpr` when switching to C++17
        auto& st = y.second;
        if (std::is_same<Select, detail::select_all>::value) {
          st.buf.insert(st.buf.end(), chunk.begin(), chunk.end());
        } else {
          Select select;
          for (auto& piece : chunk)
            if (select(st.filter, piece))
              st.buf.emplace_back(piece);
        }
        x.second->emit_batches(this->self_, st.buf, force_underfull);
      };
      detail::zip_foreach(g, this->paths_.container(), state_map_.container());
    }
  }

  state_map_type state_map_;
};

} // namespace caf

#endif // CAF_BROADCAST_SCATTERER_HPP

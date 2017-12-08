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

namespace caf {

template <class T>
class broadcast_scatterer : public buffered_scatterer<T> {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = buffered_scatterer<T>;

  /// Container for caching `T`s per path.
  using cache_type = std::vector<T>;

  /// Maps slot IDs to caches.
  using cache_map_type = detail::unordered_flat_map<stream_slots, cache_type>;

  typename super::path_ptr add_path(stream_slots slots,
                                    strong_actor_ptr target) override {
    auto res = caches_.emplace(slots, cache_type{});
    return res.second ? super::add_path(slots, target) : nullptr;
  }

  broadcast_scatterer(local_actor* selfptr) : super(selfptr) {
    // nop
  }

  void emit_batches() override {
    CAF_LOG_TRACE("");
    if (!this->delay_partial_batches()) {
      force_emit_batches();
    } else {
      auto f = [&](outbound_path&, cache_type&) {
        // Do nothing, i.e., leave the cache untouched.
      };
      emit_batches_impl(f);
    }
  }

  void force_emit_batches() override {
    CAF_LOG_TRACE("");
    auto f = [&](outbound_path& p, cache_type& c) {
      p.emit_batch(this->self_, c.size(), make_message(std::move(c)));
    };
    emit_batches_impl(f);
  }

protected:
  void about_to_erase(typename super::map_type::iterator i, bool,
                      error*) override {
    caches_.erase(i->second->slots);
  }

private:
  /// Iterates `paths_`, applying `f` to all but the last element and `g` to
  /// the last element.
  template <class F, class G>
  void iterate_paths(F& f, G& g) {
    // The vector storing all paths has the same order as the vector storing
    // all caches. This allows us to iterate both vectors using a single index.
    auto& pvec = this->paths_.container();
    auto& cvec = caches_.container();
    CAF_ASSERT(pvec.size() == cvec.size());
    size_t l = pvec.size() - 1;
    for (size_t i = 0u; i != l; ++i) {
      CAF_ASSERT(pvec[i].first == cvec[i].first);
      f(*pvec[i].second, cvec[i].second);
    }
    CAF_ASSERT(pvec[l].first == cvec[l].first);
    g(*pvec[l].second, cvec[l].second);
  }

  template <class OnPartialBatch>
  void emit_batches_impl(OnPartialBatch& f) {
    if (this->paths_.empty())
      return;
    auto emit_impl = [&](outbound_path& p, cache_type& c) {
      if (c.empty())
        return;
      auto dbs = p.desired_batch_size;
      CAF_ASSERT(dbs > 0);
      if (c.size() == dbs) {
        p.emit_batch(this->self_, c.size(), make_message(std::move(c)));
      } else {
        auto i = c.begin();
        auto e = c.end();
        while (std::distance(i, e) >= static_cast<ptrdiff_t>(dbs)) {
          std::vector<T> tmp{std::make_move_iterator(i),
                             std::make_move_iterator(i + dbs)};
          p.emit_batch(this->self_, dbs, make_message(std::move(tmp)));
          i += dbs;
        }
        if (i == e) {
          c.clear();
        } else {
          c.erase(c.begin(), i);
          f(p, c);
        }
      }
    };
    // Calculate how many more items we can put to our caches at the most.
    auto chunk_size = std::numeric_limits<size_t>::max();
    {
      auto& pvec = this->paths_.container();
      auto& cvec = caches_.container();
      CAF_ASSERT(pvec.size() == cvec.size());
      for (size_t i = 0u; i != pvec.size(); ++i) {
        chunk_size = std::min(chunk_size, pvec[i].second->open_credit
                                           - cvec[i].second.size());
      }
    }
    auto chunk = this->get_chunk(chunk_size);
    if (chunk.empty()) {
      auto g = [&](outbound_path& p, cache_type& c) {
        emit_impl(p, c);
      };
      iterate_paths(g, g);
    } else {
      auto copy_emit = [&](outbound_path& p, cache_type& c) {
        c.insert(c.end(), chunk.begin(), chunk.end());
        emit_impl(p, c);
      };
      auto move_emit = [&](outbound_path& p, cache_type& c) {
        if (c.empty())
          c = std::move(chunk);
        else
          c.insert(c.end(), std::make_move_iterator(chunk.begin()),
                   std::make_move_iterator(chunk.end()));
        emit_impl(p, c);
      };
      iterate_paths(copy_emit, move_emit);
    }
  }

  cache_map_type caches_;
};

} // namespace caf

#endif // CAF_BROADCAST_SCATTERER_HPP

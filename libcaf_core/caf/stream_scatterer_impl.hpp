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

#ifndef CAF_STREAM_SCATTERER_IMPL_HPP
#define CAF_STREAM_SCATTERER_IMPL_HPP

#include <cstddef>
#include <memory>

#include "caf/stream_scatterer.hpp"

#include "caf/detail/unordered_flat_map.hpp"

namespace caf {

/// Type-erased policy for dispatching data to sinks.
class stream_scatterer_impl : public stream_scatterer {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = stream_scatterer;

  /// Maps slots to paths.
  using map_type = detail::unordered_flat_map<stream_slot, unique_path_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit stream_scatterer_impl(local_actor* self);

  virtual ~stream_scatterer_impl();

  // -- properties -------------------------------------------------------------

  inline const map_type& paths() const {
    return paths_;
  }

  inline map_type& paths() {
    return paths_;
  }

  // -- path management --------------------------------------------------------

  size_t num_paths() const noexcept override;

  path_ptr add_path(stream_slots slots, strong_actor_ptr target) override;

  unique_path_ptr take_path(stream_slot slots) noexcept override;

  path_ptr path(stream_slot slots) noexcept override;

  void clear_paths() override;

protected:
  void for_each_path_impl(path_visitor& f) override;

  bool check_paths_impl(path_algorithm algo,
                        path_predicate& pred) const noexcept override;

  // -- member variables -------------------------------------------------------

  map_type paths_;
};

} // namespace caf

#endif // CAF_STREAM_SCATTERER_IMPL_HPP

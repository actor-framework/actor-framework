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

#include <cstddef>
#include <memory>

#include "caf/detail/core_export.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/outbound_path.hpp"
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"

namespace caf {

/// The default downstream manager base stores outbound paths in an unordered
/// map. It always takes ownership of the pahts by using unique pointers.
class CAF_CORE_EXPORT downstream_manager_base : public downstream_manager {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = downstream_manager;

  /// Maps slots to paths.
  using map_type = detail::unordered_flat_map<stream_slot, unique_path_ptr>;

  /// Optional metrics for outbound stream traffic.
  struct metrics_t {
    /// Counts the total number of elements that have been pushed downstream.
    telemetry::int_counter* pushed_elements = nullptr;

    /// Tracks how many stream elements are currently waiting in the output
    /// buffer due to insufficient credit.
    telemetry::int_gauge* output_buffer_size = nullptr;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit downstream_manager_base(stream_manager* parent);

  downstream_manager_base(stream_manager* parent, type_id_t type);

  ~downstream_manager_base() override;

  // -- properties -------------------------------------------------------------

  const map_type& paths() const {
    return paths_;
  }

  map_type& paths() {
    return paths_;
  }

  // -- path management --------------------------------------------------------

  size_t num_paths() const noexcept override;

  bool remove_path(stream_slot slots, error reason,
                   bool silent) noexcept override;

  path_ptr path(stream_slot slots) noexcept override;

  void clear_paths() override;

  // -- callbacks for actor metrics --------------------------------------------

  void generated_messages(size_t num) {
    if (num > 0 && metrics_.output_buffer_size)
      metrics_.output_buffer_size->inc(static_cast<int64_t>(num));
  }

  void dropped_messages(size_t num) {
    if (num > 0 && metrics_.output_buffer_size)
      metrics_.output_buffer_size->dec(static_cast<int64_t>(num));
  }

  void shipped_messages(size_t num) {
    if (num > 0 && metrics_.output_buffer_size) {
      metrics_.output_buffer_size->dec(static_cast<int64_t>(num));
      metrics_.pushed_elements->inc(static_cast<int64_t>(num));
    }
  }

protected:
  bool insert_path(unique_path_ptr ptr) override;

  void for_each_path_impl(path_visitor& f) override;

  bool check_paths_impl(path_algorithm algo,
                        path_predicate& pred) const noexcept override;

  // -- member variables -------------------------------------------------------

  map_type paths_;

  metrics_t metrics_;
};

} // namespace caf

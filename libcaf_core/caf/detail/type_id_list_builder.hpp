/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <cstdlib>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT type_id_list_builder {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t block_size = 8;

  // -- constructors, destructors, and assignment operators --------------------

  type_id_list_builder();

  ~type_id_list_builder();

  // -- modifiers
  // ---------------------------------------------------------------

  void reserve(size_t new_capacity);

  void push_back(type_id_t id);

  void clear() noexcept {
    if (storage_)
      size_ = 1;
  }

  // -- properties -------------------------------------------------------------

  size_t size() const noexcept;

  /// @pre `index < size()`
  type_id_t operator[](size_t index) const noexcept;

  // -- iterator access --------------------------------------------------------

  auto begin() const noexcept {
    return storage_;
  }

  auto end() const noexcept {
    return storage_ + size_;
  }

  // -- conversions ------------------------------------------------------------

  /// Convertes the internal buffer to a @ref type_id_list and returns it.
  type_id_list move_to_list() noexcept;

  /// Convertes the internal buffer to a @ref type_id_list and returns it.
  type_id_list copy_to_list() const;

private:
  size_t size_;
  size_t reserved_;
  type_id_t* storage_;
};

} // namespace caf::detail

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

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

#include "caf/fwd.hpp"
#include "caf/uri.hpp"
#include "caf/ref_counted.hpp"
#include "caf/string_view.hpp"

namespace caf {
namespace detail {

class uri_impl {
public:
  // -- constructors, destructors, and assignment operators --------------------

  uri_impl();

  uri_impl(const uri_impl&) = delete;

  uri_impl& operator=(const uri_impl&) = delete;

  // -- member variables -------------------------------------------------------

  static uri_impl default_instance;

  /// Null-terminated buffer for holding the string-representation of the URI.
  std::string str;

  /// Scheme component.
  std::string scheme;

  /// Assembled authority component.
  uri::authority_type authority;

  /// Path component.
  std::string path;

  /// Query component as key-value pairs.
  uri::query_map query;

  /// The fragment component.
  std::string fragment;

  // -- modifiers --------------------------------------------------------------

  /// Assembles the human-readable string representation for this URI.
  void assemble_str();

  // -- friend functions -------------------------------------------------------

  friend void intrusive_ptr_add_ref(const uri_impl* p);

  friend void intrusive_ptr_release(const uri_impl* p);

private:
  // -- member variables -------------------------------------------------------

  mutable std::atomic<size_t> rc_;
};

} // namespace detail
} // namespace caf

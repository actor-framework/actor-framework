/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_TYPE_ERASED_COLLECTION_HPP
#define CAF_TYPE_ERASED_COLLECTION_HPP

#include <cstddef>
#include <cstdint>
#include <typeinfo>

#include "caf/fwd.hpp"

namespace caf {

/// Represents a tuple of type-erased values.
class type_erased_tuple {
public:
  using rtti_pair = std::pair<uint16_t, const std::type_info*>;

  virtual ~type_erased_tuple();

  // pure virtual modifiers

  virtual void* get_mutable(size_t pos) = 0;

  virtual void load(size_t pos, deserializer& source) = 0;

  // pure virtual observers

  virtual size_t size() const = 0;

  virtual uint32_t type_token() const = 0;

  virtual rtti_pair type(size_t pos) const = 0;

  virtual const void* get(size_t pos) const = 0;

  virtual std::string stringify(size_t pos) const = 0;

  virtual void save(size_t pos, serializer& sink) const = 0;

  // observers

  /// Checks whether the type of the stored value matches
  /// the type nr and type info object.
  bool matches(size_t pos, uint16_t tnr, const std::type_info* tinf) const;

  // inline observers

  inline uint16_t type_nr(size_t pos) const {
    return type(pos).first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  inline bool matches(size_t pos, const rtti_pair& rtti) const {
    return matches(pos, rtti.first, rtti.second);
  }
};

} // namespace caf

#endif // CAF_TYPE_ERASED_COLLECTION_HPP

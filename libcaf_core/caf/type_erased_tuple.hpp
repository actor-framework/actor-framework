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

#include <tuple>
#include <cstddef>
#include <cstdint>
#include <typeinfo>

#include "caf/fwd.hpp"
#include "caf/type_erased_value.hpp"

#include "caf/detail/type_nr.hpp"

namespace caf {

/// Represents a tuple of type-erased values.
class type_erased_tuple {
public:
  // -- member types -----------------------------------------------------------

  using rtti_pair = std::pair<uint16_t, const std::type_info*>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~type_erased_tuple();

  // -- pure virtual modifiers -------------------------------------------------

  /// Returns a mutable pointer to the element at position `pos`.
  virtual void* get_mutable(size_t pos) = 0;

  /// Load the content for the element at position `pos` from `source`.
  virtual void load(size_t pos, deserializer& source) = 0;

  // -- modifiers --------------------------------------------------------------

  /// Load the content for the tuple from `source`.
  void load(deserializer& source);

  // -- pure virtual observers -------------------------------------------------

  /// Returns the size of this tuple.
  virtual size_t size() const = 0;

  /// Returns a type hint for the element types.
  virtual uint32_t type_token() const = 0;

  /// Returns the type number and `std::type_info` object for
  /// the element at position `pos`.
  virtual rtti_pair type(size_t pos) const = 0;

  /// Returns the element at position `pos`.
  virtual const void* get(size_t pos) const = 0;

  /// Returns a string representation of the element at position `pos`.
  virtual std::string stringify(size_t pos) const = 0;

  /// Returns a copy of the element at position `pos`.
  virtual type_erased_value_ptr copy(size_t pos) const = 0;

  /// Saves the element at position `pos` to `sink`.
  virtual void save(size_t pos, serializer& sink) const = 0;

  // -- observers --------------------------------------------------------------

  /// Returns a string representation of the tuple.
  std::string stringify() const;

  /// Saves the content of the tuple to `sink`.
  void save(serializer& sink) const;

  /// Checks whether the type of the stored value matches
  /// the type nr and type info object.
  bool matches(size_t pos, uint16_t tnr, const std::type_info* tinf) const;

  // -- inline observers -------------------------------------------------------

  /// Returns the type number for the element at position `pos`.
  inline uint16_t type_nr(size_t pos) const {
    return type(pos).first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  inline bool matches(size_t pos, const rtti_pair& rtti) const {
    return matches(pos, rtti.first, rtti.second);
  }
};


/// @relates type_erased_tuple
template <class Processor>
typename std::enable_if<Processor::is_saving::value>::type
serialize(Processor& proc, type_erased_tuple& x) {
  x.save(proc);
}

/// @relates type_erased_tuple
template <class Processor>
typename std::enable_if<Processor::is_loading::value>::type
serialize(Processor& proc, type_erased_tuple& x) {
  x.load(proc);
}

/// @relates type_erased_tuple
inline std::string to_string(const type_erased_tuple& x) {
  return x.stringify();
}

template <class... Ts>
class type_erased_tuple_view : public type_erased_tuple {
public:
  // -- member types -----------------------------------------------------------
  template <size_t X>
  using num_token = std::integral_constant<size_t, X>;

  // -- constructors, destructors, and assignment operators --------------------

  type_erased_tuple_view(Ts&... xs) : xs_(xs...) {
    init();
  }

  type_erased_tuple_view(const type_erased_tuple_view& other) : xs_(other.xs_) {
    init();
  }

  // -- overridden modifiers ---------------------------------------------------

  void* get_mutable(size_t pos) override {
    return ptrs_[pos]->get_mutable();
  }

  void load(size_t pos, deserializer& source) override {
    ptrs_[pos]->load(source);
  }

  // -- overridden observers ---------------------------------------------------

  size_t size() const override {
    return sizeof...(Ts);
  }

  uint32_t type_token() const override {
    return detail::make_type_token<Ts...>();
  }

  rtti_pair type(size_t pos) const override {
    return ptrs_[pos]->type();
  }

  const void* get(size_t pos) const override {
    return ptrs_[pos]->get();
  }

  std::string stringify(size_t pos) const override {
    return ptrs_[pos]->stringify();
  }

  type_erased_value_ptr copy(size_t pos) const override {
    return ptrs_[pos]->copy();
  }

  void save(size_t pos, serializer& sink) const override {
    return ptrs_[pos]->save(sink);
  }

private:
  // -- pointer "lookup table" utility -----------------------------------------

  template <size_t N>
  void init(num_token<N>, num_token<N>) {
    // end of recursion
  }

  template <size_t P, size_t N>
  void init(num_token<P>, num_token<N> last) {
    ptrs_[P] = &std::get<P>(xs_);
    init(num_token<P + 1>{}, last);
  }

  void init() {
    init(num_token<0>{}, num_token<sizeof...(Ts)>{});
  }

  // -- data members -----------------------------------------------------------

  std::tuple<type_erased_value_impl<std::reference_wrapper<Ts>>...> xs_;
  type_erased_value* ptrs_[sizeof...(Ts)];
};

template <class... Ts>
type_erased_tuple_view<Ts...> make_type_erased_tuple_view(Ts&... xs) {
  return {xs...};
}

} // namespace caf

#endif // CAF_TYPE_ERASED_COLLECTION_HPP

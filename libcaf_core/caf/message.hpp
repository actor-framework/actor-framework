/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_MESSAGE_HPP
#define CAF_MESSAGE_HPP

#include <type_traits>

#include "caf/config.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

class message_handler;

/**
 * Describes a fixed-length copy-on-write tuple with elements of any type.
 */
class message {

 public:

  /**
   * A raw pointer to the data.
   */
  using raw_ptr = detail::message_data*;

  /**
   * A (COW) smart pointer to the data.
   */
  using data_ptr = detail::message_data::ptr;

  /**
   * An iterator to access each element as `const void*.
   */
  using const_iterator = detail::message_data::const_iterator;

  /**
   * Creates an empty tuple.
   */
  message() = default;

  /**
   * Move constructor.
   */
  message(message&&);

  /**
   * Copy constructor.
   */
  message(const message&) = default;

  /**
   * Move assignment.
   */
  message& operator=(message&&);

  /**
   * Copy assignment.
   */
  message& operator=(const message&) = default;

  /**
   * Gets the size of this tuple.
   */
  inline size_t size() const {
    return m_vals ? m_vals->size() : 0;
  }

  /**
   * Creates a new tuple with all but the first n values.
   */
  message drop(size_t n) const;

  /**
   * Creates a new tuple with all but the last n values.
   */
  message drop_right(size_t n) const;

  /**
   * Creates a new tuple from the first n values.
   */
  inline message take(size_t n) const {
    return n >= size() ? *this : drop_right(size() - n);
  }

  /**
   * Creates a new tuple from the last n values.
   */
  inline message take_right(size_t n) const {
    return n >= size() ? *this : drop(size() - n);
  }

  /**
   * Gets a mutable pointer to the element at position @p p.
   */
  void* mutable_at(size_t p);

  /**
   * Gets a const pointer to the element at position @p p.
   */
  const void* at(size_t p) const;

  /**
   * Gets {@link uniform_type_info uniform type information}
   * of the element at position @p p.
   */
  const uniform_type_info* type_at(size_t p) const;

  /**
   * Returns true if this message has the types @p Ts.
   */
  template <class... Ts>
  bool has_types() const {
    if (size() != sizeof...(Ts)) {
      return false;
    }
    const std::type_info* ts[] = {&typeid(Ts)...};
    for (size_t i = 0; i < sizeof...(Ts); ++i) {
      if (!type_at(i)->equal_to(*ts[i])) {
        return false;
      }
    }
    return true;
  }

  /**
   * Returns @c true if `*this == other, otherwise false.
   */
  bool equals(const message& other) const;

  /**
   * Returns true if `size() == 0, otherwise false.
   */
  inline bool empty() const {
    return size() == 0;
  }

  /**
   * Returns the value at @p as instance of @p T.
   */
  template <class T>
  inline const T& get_as(size_t p) const {
    CAF_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<const T*>(at(p));
  }

  /**
   * Returns the value at @p as mutable data_ptr& of type @p T&.
   */
  template <class T>
  inline T& get_as_mutable(size_t p) {
    CAF_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<T*>(mutable_at(p));
  }

  /**
   * Returns an iterator to the beginning.
   */
  inline const_iterator begin() const {
    return m_vals->begin();
  }

  /**
   * Returns an iterator to the end.
   */
  inline const_iterator end() const {
    return m_vals->end();
  }

  /**
   * Returns a copy-on-write pointer to the internal data.
   */
  inline data_ptr& vals() {
    return m_vals;
  }

  /**
   * Returns a const copy-on-write pointer to the internal data.
   */
  inline const data_ptr& vals() const {
    return m_vals;
  }

  /**
   * Returns a const copy-on-write pointer to the internal data.
   */
  inline const data_ptr& cvals() const {
    return m_vals;
  }

  /**
   * Returns either `&typeid(detail::type_list<Ts...>)`, where
   * `Ts...` are the element types, or `&typeid(void)`.
   *
   * The type token `&typeid(void)` indicates that this tuple is dynamically
   * typed, i.e., the types where not available at compile time.
   */
  const std::type_info* type_token() const;

  /**
   * Checks whether this tuple is dynamically typed, i.e.,
   * its types were not known at compile time.
   */
  bool dynamically_typed() const;

  /**
   * Applies @p handler to this message and returns the result
   *  of `handler(*this)`.
   */
  optional<message> apply(message_handler handler);

  /** @cond PRIVATE */

  inline void force_detach() {
    m_vals.detach();
  }

  void reset();

  explicit message(raw_ptr);

  inline const std::string* tuple_type_names() const {
    return m_vals->tuple_type_names();
  }

  explicit message(const data_ptr& vals);

  struct move_from_tuple_helper {
    template <class... Ts>
    inline message operator()(Ts&... vs) {
      return make_message(std::move(vs)...);
    }
  };

  template <class... Ts>
  inline message move_from_tuple(std::tuple<Ts...>&& tup) {
    move_from_tuple_helper f;
    return detail::apply_args(f, detail::get_indices(tup), tup);
  }

  /** @endcond */

 private:

  data_ptr m_vals;

};

/**
 * @relates message
 */
inline bool operator==(const message& lhs, const message& rhs) {
  return lhs.equals(rhs);
}

/**
 * @relates message
 */
inline bool operator!=(const message& lhs, const message& rhs) {
  return !(lhs == rhs);
}

/**
 * Creates a new `message` containing the elements `args...`.
 * @relates message
 */
template <class T, class... Ts>
typename std::enable_if<
  !std::is_same<message, typename std::decay<T>::type>::value
  || (sizeof...(Ts) > 0),
  message
>::type
make_message(T&& arg, Ts&&... args) {
  using namespace detail;
  using data = tuple_vals<typename strip_and_convert<T>::type,
                          typename strip_and_convert<Ts>::type...>;
  auto ptr = new data(std::forward<T>(arg), std::forward<Ts>(args)...);
  return message{detail::message_data::ptr{ptr}};
}

/**
 * Returns a copy of @p other.
 * @relates message
 */
inline message make_message(message other) {
  return std::move(other);
}

} // namespace caf

#endif // CAF_MESSAGE_HPP

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

#ifndef CPPA_COW_TUPLE_HPP
#define CPPA_COW_TUPLE_HPP

// <backward_compatibility version="0.9" whole_file="yes">
#include <cstddef>
#include <string>
#include <typeinfo>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/decorated_tuple.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

template <class... Ts>
class cow_tuple;

template <class Head, class... Tail>
class cow_tuple<Head, Tail...> {

  static_assert(detail::tl_forall<detail::type_list<Head, Tail...>,
                     detail::is_legal_tuple_type
                    >::value,
          "illegal types in cow_tuple definition: "
          "pointers and references are prohibited");

  using data_type =
    detail::tuple_vals<typename detail::strip_and_convert<Head>::type,
                       typename detail::strip_and_convert<Tail>::type...>;

  using data_ptr = detail::message_data::ptr;

 public:

  using types = detail::type_list<Head, Tail...>;

  static constexpr size_t num_elements = sizeof...(Tail) + 1;

  cow_tuple() : m_vals(new data_type) {
    // nop
  }

  template <class... Ts>
  cow_tuple(Head arg, Ts&&... args)
      : m_vals(new data_type(std::move(arg), std::forward<Ts>(args)...)) {
    // nop
  }

  cow_tuple(cow_tuple&&) = default;
  cow_tuple(const cow_tuple&) = default;
  cow_tuple& operator=(cow_tuple&&) = default;
  cow_tuple& operator=(const cow_tuple&) = default;

  inline size_t size() const {
    return sizeof...(Tail) + 1;
  }

  inline const void* at(size_t p) const {
    return m_vals->at(p);
  }

  inline void* mutable_at(size_t p) {
    return m_vals->mutable_at(p);
  }

  inline const uniform_type_info* type_at(size_t p) const {
    return m_vals->type_at(p);
  }

  inline cow_tuple<Tail...> drop_left() const {
    return cow_tuple<Tail...>::offset_subtuple(m_vals, 1);
  }

  inline operator message() {
    return message{m_vals};
  }

  static cow_tuple from(message& msg) {
    return cow_tuple(msg.vals());
  }

 private:

  cow_tuple(data_ptr ptr) : m_vals(ptr) { }

  data_type* ptr() {
    return static_cast<data_type*>(m_vals.get());
  }

  const data_type* ptr() const {
    return static_cast<data_type*>(m_vals.get());
  }

  data_ptr m_vals;

};

template <size_t N, class... Ts>
const typename detail::type_at<N, Ts...>::type&
get(const cow_tuple<Ts...>& tup) {
  using result_type = typename detail::type_at<N, Ts...>::type;
  return *reinterpret_cast<const result_type*>(tup.at(N));
}

template <size_t N, class... Ts>
typename detail::type_at<N, Ts...>::type&
get_ref(cow_tuple<Ts...>& tup) {
  using result_type = typename detail::type_at<N, Ts...>::type;
  return *reinterpret_cast<result_type*>(tup.mutable_at(N));
}

template <class... Ts>
cow_tuple<typename detail::strip_and_convert<Ts>::type...>
make_cow_tuple(Ts&&... args) {
  return {std::forward<Ts>(args)...};
}

template <class TypeList>
struct cow_tuple_from_type_list;

template <class... Ts>
struct cow_tuple_from_type_list<detail::type_list<Ts...>> {
  typedef cow_tuple<Ts...> type;
};

} // namespace caf
// </backward_compatibility>

#endif // CPPA_COW_TUPLE_HPP

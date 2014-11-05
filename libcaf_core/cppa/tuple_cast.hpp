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


#ifndef CPPA_TUPLE_CAST_HPP
#define CPPA_TUPLE_CAST_HPP

// <backward_compatibility version="0.9" whole_file="yes">
#include <type_traits>

#include "caf/message.hpp"
#include "caf/optional.hpp"
#include "caf/wildcard_position.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/types_array.hpp"
#include "caf/detail/decorated_tuple.hpp"

namespace caf {

template <class TupleIter, class PatternIter,
     class Push, class Commit, class Rollback>
bool dynamic_match(TupleIter tbegin, TupleIter tend,
          PatternIter pbegin, PatternIter pend,
          Push& push, Commit& commit, Rollback& rollback) {
  while (!(pbegin == pend && tbegin == tend)) {
    if (pbegin == pend) {
      // reached end of pattern while some values remain unmatched
      return false;
    }
    else if (*pbegin == nullptr) { // nullptr == wildcard (anything)
      // perform submatching
      ++pbegin;
      // always true at the end of the pattern
      if (pbegin == pend) return true;
      // safe current mapping as fallback
      commit();
      // iterate over tuple values until we found a match
      for (; tbegin != tend; ++tbegin) {
        if (dynamic_match(tbegin, tend, pbegin, pend,
                  push, commit, rollback)) {
          return true;
        }
        // restore mapping to fallback (delete invalid mappings)
        rollback();
      }
      return false; // no submatch found
    }
    // compare types
    else if (tbegin.type() == *pbegin) push(tbegin);
    // no match
    else return false;
    // next iteration
    ++tbegin;
    ++pbegin;
  }
  return true; // pbegin == pend && tbegin == tend
}

template <class... T>
auto moving_tuple_cast(message& tup)
  -> optional<
    typename cow_tuple_from_type_list<
      typename detail::tl_filter_not<detail::type_list<T...>,
                     is_anything>::type
    >::type> {
  using result_type =
    typename cow_tuple_from_type_list<
      typename detail::tl_filter_not<detail::type_list<T...>, is_anything>::type
    >::type;
  using types = detail::type_list<T...>;
  static constexpr auto impl =
      get_wildcard_position<detail::type_list<T...>>();
  auto& tarr = detail::static_types_array<T...>::arr;
  const uniform_type_info* const* arr_pos = tarr.begin();
  message sub;
  switch (impl) {
    case wildcard_position::nil: {
      sub = tup;
      break;
    }
    case wildcard_position::trailing: {
      sub = tup.take(sizeof...(T) - 1);
      break;
    }
    case wildcard_position::leading: {
      ++arr_pos; // skip leading 'anything'
      sub = tup.take_right(sizeof...(T) - 1);
      break;
    }
    case wildcard_position::in_between:
    case wildcard_position::multiple: {
      constexpr size_t wc_count =
        detail::tl_count<detail::type_list<T...>, is_anything>::value;
      if (tup.size() >= (sizeof...(T) - wc_count)) {
        std::vector<size_t> mv;  // mapping vector
        size_t commited_size = 0;
        auto fpush = [&](const typename message::const_iterator& iter) {
          mv.push_back(iter.position());
        };
        auto fcommit = [&] {
          commited_size = mv.size();
        };
        auto frollback = [&] {
          mv.resize(commited_size);
        };
        if (dynamic_match(tup.begin(), tup.end(),
                  tarr.begin(), tarr.end(),
                  fpush, fcommit, frollback)) {
          message msg{detail::decorated_tuple::create(
                  tup.vals(), std::move(mv))};
          return result_type::from(msg);
        }
        return none;
      }
      break;
    }
  }
  // same for nil, leading, and trailing
  if (std::equal(sub.begin(), sub.end(),
           arr_pos, detail::types_only_eq)) {
    return result_type::from(sub);
  }
  return none;
}

template <class... T>
auto moving_tuple_cast(message& tup, const detail::type_list<T...>&)
  -> decltype(moving_tuple_cast<T...>(tup)) {
  return moving_tuple_cast<T...>(tup);
}

template <class... T>
auto tuple_cast(message tup)
   -> optional<
      typename cow_tuple_from_type_list<
      typename detail::tl_filter_not<detail::type_list<T...>,
                     is_anything>::type
    >::type> {
  return moving_tuple_cast<T...>(tup);
}

template <class... T>
auto tuple_cast(message tup, const detail::type_list<T...>&)
  -> decltype(tuple_cast<T...>(tup)) {
  return moving_tuple_cast<T...>(tup);
}

template <class... T>
auto unsafe_tuple_cast(message& tup, const detail::type_list<T...>&)
  -> decltype(tuple_cast<T...>(tup)) {
  return tuple_cast<T...>(tup);
}

} // namespace caf
// </backward_compatibility>

#endif // CPPA_TUPLE_CAST_HPP

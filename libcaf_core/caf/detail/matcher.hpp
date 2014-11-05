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

#ifndef CAF_DETAIL_MATCHES_HPP
#define CAF_DETAIL_MATCHES_HPP

#include <numeric>

#include "caf/message.hpp"
#include "caf/wildcard_position.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/pseudo_tuple.hpp"

namespace caf {
namespace detail {

template <class Pattern, class FilteredPattern>
struct matcher;

template <class... Ts, class... Us>
struct matcher<type_list<Ts...>, type_list<Us...>> {
  template <class TupleIter, class PatternIter, class Push, class Commit,
            class Rollback>
  bool operator()(TupleIter tbegin, TupleIter tend, PatternIter pbegin,
                  PatternIter pend, Push& push, Commit& commit,
                  Rollback& rollback) const {
    while (!(pbegin == pend && tbegin == tend)) {
      if (pbegin == pend) {
        // reached end of pattern while some values remain unmatched
        return false;
      }
      if (*pbegin == nullptr) { // nullptr == wildcard (anything)
        // perform submatching
        ++pbegin;
        // always true at the end of the pattern
        if (pbegin == pend) {
          return true;
        }
        // safe current mapping as fallback
        commit();
        // iterate over tuple values until we found a match
        for (; tbegin != tend; ++tbegin) {
          if ((*this)(tbegin, tend, pbegin, pend, push, commit, rollback)) {
            return true;
          }
          // restore mapping to fallback (delete invalid mappings)
          rollback();
        }
        return false; // no submatch found
      }
      // compare types
      if (tbegin.type() != *pbegin) {
        // type mismatch
        return false;
      }
      // next iteration
      push(tbegin);
      ++tbegin;
      ++pbegin;
    }
    return true; // pbegin == pend && tbegin == tend
  }

  bool operator()(const message& tup, pseudo_tuple<Us...>* out) const {
    auto& tarr = static_types_array<Ts...>::arr;
    if (sizeof...(Us) == 0) {
      // this pattern only has wildcards and thus always matches
      return true;
    }
    if (tup.size() < sizeof...(Us)) {
      return false;
    }
    if (out) {
      size_t pos = 0;
      size_t fallback_pos = 0;
      auto fpush = [&](const typename message::const_iterator& iter) {
        (*out)[pos++] = const_cast<void*>(iter.value());
      };
      auto fcommit = [&] { fallback_pos = pos; };
      auto frollback = [&] { pos = fallback_pos; };
      return (*this)(tup.begin(), tup.end(), tarr.begin(), tarr.end(), fpush,
                     fcommit, frollback);
    }
    auto no_push = [](const typename message::const_iterator&) { };
    auto nop = [] { };
    return (*this)(tup.begin(), tup.end(), tarr.begin(), tarr.end(), no_push,
                   nop, nop);
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MATCHES_HPP

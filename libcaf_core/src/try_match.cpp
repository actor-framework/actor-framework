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

#include "caf/detail/try_match.hpp"

namespace caf {
namespace detail {

using pattern_iterator = const meta_element*;

bool is_wildcard(const meta_element& me) {
  return me.typenr == 0 && me.type == nullptr;
}

bool match_element(const meta_element& me, const message_iterator& iter,
                   void** storage) {
  CAF_REQUIRE(me.typenr != 0 || me.type != nullptr);
  if (!iter.match_element(me.typenr, me.type)) {
    return false;
  }
  if (storage) {
    *storage = const_cast<void*>(iter.value());
  }
  return true;
}

bool match_atom_constant(const meta_element& me, const message_iterator& iter,
                         void** storage) {
  CAF_REQUIRE(me.typenr == detail::type_nr<atom_value>::value);
  if (!iter.match_element(detail::type_nr<atom_value>::value, nullptr)) {
    return false;
  }
  if (storage) {
    if (iter.value_as<atom_value>() != me.v) {
      return false;
    }
    // This assignment casts `atom_value` to `atom_constant<V>*`.
    // This type violation could theoretically cause undefined behavior.
    // However, `uti` does have an address that is guaranteed to be valid
    // throughout the runtime of the program and the atom constant
    // does not have any members. Hence, this is nonetheless safe since
    // we are never actually going to dereference the pointer.
    *storage = const_cast<void*>(iter.value());
  }
  return true;
}

class set_commit_rollback {
 public:
  using pointer = void**;
  set_commit_rollback(const set_commit_rollback&) = delete;
  set_commit_rollback& operator=(const set_commit_rollback&) = delete;
  set_commit_rollback(pointer ptr) : m_data(ptr), m_pos(0), m_fallback_pos(0) {
    // nop
  }
  inline void inc() {
    ++m_pos;
  }
  inline pointer current() {
    return m_data? &m_data[m_pos] : nullptr;
  }
  inline void commit() {
    m_fallback_pos = m_pos;
  }
  inline void rollback() {
    m_pos = m_fallback_pos;
  }
 private:
  pointer m_data;
  size_t m_pos;
  size_t m_fallback_pos;
};

bool try_match(message_iterator mbegin, message_iterator mend,
               pattern_iterator pbegin, pattern_iterator pend,
               set_commit_rollback& storage) {
  while (mbegin != mend) {
    if (pbegin == pend) {
      return false;
    }
    if (is_wildcard(*pbegin)) {
      // perform submatching
      ++pbegin;
      // always true at the end of the pattern
      if (pbegin == pend) {
        return true;
      }
      // safe current mapping as fallback
      storage.commit();
      // iterate over remaining values until we found a match
      for (; mbegin != mend; ++mbegin) {
        if (try_match(mbegin, mend, pbegin, pend, storage)) {
          return true;
        }
        // restore mapping to fallback (delete invalid mappings)
        storage.rollback();
      }
      return false; // no submatch found
    }
    // inspect current element
    if (!pbegin->fun(*pbegin, mbegin, storage.current())) {
      // type mismatch
      return false;
    }
    // next iteration
    storage.inc();
    ++mbegin;
    ++pbegin;
  }
  // we found a match if we've inspected each element and consumed
  // the whole pattern (or the remainder consists of wildcards only)
  return std::all_of(pbegin, pend, is_wildcard);
}

bool try_match(const message& msg, pattern_iterator pb, size_t ps, void** out) {
  set_commit_rollback scr{out};
  auto res = try_match(msg.begin(), msg.end(), pb, pb + ps, scr);
  return res;
}

} // namespace detail
} // namespace caf

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

#include "caf/detail/try_match.hpp"

namespace caf {
namespace detail {

using pattern_iterator = const meta_element*;

bool is_wildcard(const meta_element& me) {
  return me.typenr == 0 && me.type == nullptr;
}

bool match_element(const meta_element& me, const message& msg,
                   size_t pos, void** storage) {
  CAF_ASSERT(me.typenr != 0 || me.type != nullptr);
  if (! msg.match_element(pos, me.typenr, me.type)) {
    return false;
  }
  *storage = const_cast<void*>(msg.at(pos));
  return true;
}

bool match_atom_constant(const meta_element& me, const message& msg,
                         size_t pos, void** storage) {
  CAF_ASSERT(me.typenr == detail::type_nr<atom_value>::value);
  if (! msg.match_element(pos, detail::type_nr<atom_value>::value, nullptr)) {
    return false;
  }
  auto ptr = msg.at(pos);
  if (me.v != *reinterpret_cast<const atom_value*>(ptr)) {
    return false;
  }
  // This assignment casts `atom_value` to `atom_constant<V>*`.
  // This type violation could theoretically cause undefined behavior.
  // However, `uti` does have an address that is guaranteed to be valid
  // throughout the runtime of the program and the atom constant
  // does not have any members. Hence, this is nonetheless safe since
  // we are never actually going to dereference the pointer.
  *storage = const_cast<void*>(ptr);
  return true;
}

class set_commit_rollback {
public:
  using pointer = void**;
  set_commit_rollback(const set_commit_rollback&) = delete;
  set_commit_rollback& operator=(const set_commit_rollback&) = delete;
  explicit set_commit_rollback(pointer ptr)
      : data_(ptr),
        pos_(0),
        fallback_pos_(0) {
    // nop
  }
  inline void inc() {
    ++pos_;
  }
  inline pointer current() const {
    return &data_[pos_];
  }
  inline void commit() {
    fallback_pos_ = pos_;
  }
  inline void rollback() {
    pos_ = fallback_pos_;
  }
private:
  pointer data_;
  size_t pos_;
  size_t fallback_pos_;
};

bool try_match(const message& msg, size_t msg_pos, size_t msg_size,
               pattern_iterator pbegin, pattern_iterator pend,
               set_commit_rollback& storage) {
  while (msg_pos < msg_size) {
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
      for (; msg_pos < msg_size; ++msg_pos) {
        if (try_match(msg, msg_pos, msg_size, pbegin, pend, storage)) {
          return true;
        }
        // restore mapping to fallback (delete invalid mappings)
        storage.rollback();
      }
      return false; // no submatch found
    }
    // inspect current element
    if (! pbegin->fun(*pbegin, msg, msg_pos, storage.current())) {
      // type mismatch
      return false;
    }
    // next iteration
    storage.inc();
    ++msg_pos;
    ++pbegin;
  }
  // we found a match if we've inspected each element and consumed
  // the whole pattern (or the remainder consists of wildcards only)
  return std::all_of(pbegin, pend, is_wildcard);
}

bool try_match(const message& msg, pattern_iterator pb, size_t ps, void** out) {
  CAF_ASSERT(out != nullptr);
  CAF_ASSERT(msg.empty() || msg.vals()->get_reference_count() > 0);
  set_commit_rollback scr{out};
  return try_match(msg, 0, msg.size(), pb, pb + ps, scr);
}

} // namespace detail
} // namespace caf

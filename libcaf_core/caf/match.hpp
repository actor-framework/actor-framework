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

#ifndef CAF_MATCH_HPP
#define CAF_MATCH_HPP

#include <vector>
#include <istream>
#include <iterator>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/match_expr.hpp"
#include "caf/skip_message.hpp"
#include "caf/message_handler.hpp"
#include "caf/message_builder.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

class match_helper {

  match_helper(const match_helper&) = delete;
  match_helper& operator=(const match_helper&) = delete;

 public:

  match_helper(match_helper&&) = default;

  inline match_helper(message t) : tup(std::move(t)) { }

  template <class... Ts>
  auto operator()(Ts&&... args)
  -> decltype(match_expr_collect(std::forward<Ts>(args)...)(message{})) {
    static_assert(sizeof...(Ts) > 0, "at least one argument required");
    auto tmp = match_expr_collect(std::forward<Ts>(args)...);
    return tmp(tup);
  }

 private:

  message tup;

};

template <class T, typename InputIterator>
class stream_matcher {

 public:

  using iterator = InputIterator;

  stream_matcher(iterator first, iterator last) : m_pos(first), m_end(last) {
    // nop
  }

  template <class... Ts>
  bool operator()(Ts&&... args) {
    auto mexpr = match_expr_collect(std::forward<Ts>(args)...);
    //TODO: static_assert -> mexpr must not have a wildcard
    constexpr size_t max_handler_args = 0; // TODO: get from mexpr
    message_handler handler = mexpr;
    while (m_pos != m_end) {
      m_mb.append(*m_pos++);
      if (m_mb.apply(handler)) {
        m_mb.clear();
      } else if (m_mb.size() == max_handler_args) {
        return false;
      }
    }
    // we have a match if all elements were consumed
    return m_mb.empty();
  }

 private:

  iterator m_pos;
  iterator m_end;
  message_builder m_mb;

};

struct identity_fun {
  template <class T>
  inline T& operator()(T& arg) { return arg; }
};

template <class InputIterator, typename Transformation = identity_fun>
class match_each_helper {

 public:

  using iterator = InputIterator;

  match_each_helper(iterator first, iterator last, Transformation fun)
      : m_pos(first), m_end(last), m_fun(std::move(fun)) {
    // nop
  }

  template <class... Ts>
  bool operator()(Ts&&... args) {
    message_handler handler = match_expr_collect(std::forward<Ts>(args)...);
    for ( ; m_pos != m_end; ++m_pos) {
      if (!handler(m_fun(*m_pos))) return false;
    }
    return true;
  }

 private:

  iterator m_pos;
  iterator m_end;
  Transformation m_fun;

};

} // namespace detail
} // namespace caf

namespace caf {

/**
 * Starts a match expression.
 * @param what Tuple that should be matched against a pattern.
 * @returns A helper object providing `operator(...).
 */
inline detail::match_helper match(message what) {
  return what;
}

/**
 * Starts a match expression.
 * @param what Value that should be matched against a pattern.
 * @returns A helper object providing `operator(...).
 */
template <class T>
detail::match_helper match(T&& what) {
  return message_builder{}.append(std::forward<T>(what)).to_message();
}

/**
 * Splits `str` using `delim` and match the resulting strings.
 */
detail::match_helper
match_split(const std::string& str, char delim, bool keep_empties = false);

/**
 * Starts a match expression that matches each element in
 *    range [first, last).
 * @param first Iterator to the first element.
 * @param last Iterator to the last element (excluded).
 * @returns A helper object providing `operator(...).
 */
template <class InputIterator>
detail::match_each_helper<InputIterator>
match_each(InputIterator first, InputIterator last) {
  return {first, last, detail::identity_fun{}};
}

/**
 * Starts a match expression that matches `proj(i) for
 *    each element `i` in range [first, last).
 * @param first Iterator to the first element.
 * @param last Iterator to the last element (excluded).
 * @param proj Projection or extractor functor.
 * @returns A helper object providing `operator(...).
 */
template <class InputIterator, typename Projection>
detail::match_each_helper<InputIterator, Projection>
match_each(InputIterator first, InputIterator last, Projection proj) {
  return {first, last, std::move(proj)};
}

template <class T>
detail::stream_matcher<T, std::istream_iterator<T>>
match_stream(std::istream& stream) {
  std::istream_iterator<T> first(stream);
  std::istream_iterator<T> last; // 'end of stream' iterator
  return {first, last};
}

template <class T, typename InputIterator>
detail::stream_matcher<T, InputIterator>
match_stream(InputIterator first, InputIterator last) {
  return {first, last};
}

} // namespace caf

#endif // CAF_MATCH_HPP

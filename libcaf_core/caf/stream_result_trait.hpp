/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://openresult.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_STREAM_RESULT_TRAIT_HPP
#define CAF_STREAM_RESULT_TRAIT_HPP

#include "caf/unit.hpp"
#include "caf/behavior.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class F>
struct stream_result_trait;

template <class T>
struct stream_result_trait<void (expected<T>)> {
  using type = T;
  template <class OnResult>
  static behavior make_result_handler(OnResult f) {
    return {
      [=](T& res) {
        f(std::move(res));
      },
      [=](error& err) {
        f(std::move(err));
      }
    };
  }
};

template <>
struct stream_result_trait<void (expected<void>)> {
  using type = void;
  template <class OnResult>
  static behavior make_result_handler(OnResult f) {
    return {
      [=]() {
        f(unit);
      },
      [=](error& err) {
        f(std::move(err));
      }
    };
  }
};

template <class F>
using stream_result_trait_t =
  stream_result_trait<typename detail::get_callable_trait<F>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_RESULT_TRAIT_HPP

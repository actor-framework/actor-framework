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

#ifndef CAF_DETAIL_BEHAVIOR_IMPL_HPP
#define CAF_DETAIL_BEHAVIOR_IMPL_HPP

#include <tuple>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/variant.hpp"
#include "caf/optional.hpp"
#include "caf/match_case.hpp"
#include "caf/make_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/atom.hpp"
#include "caf/either.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/ref_counted.hpp"
#include "caf/skip_message.hpp"
#include "caf/response_promise.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/tail_argument_token.hpp"
#include "caf/detail/optional_message_visitor.hpp"

namespace caf {

class message_handler;
using bhvr_invoke_result = optional<message>;

} // namespace caf

namespace caf {
namespace detail {

template <class... Ts>
struct has_skip_message {
  static constexpr bool value =
    disjunction<std::is_same<Ts, skip_message_t>::value...>::value;
};

class behavior_impl : public ref_counted {
public:
  using pointer = intrusive_ptr<behavior_impl>;

  ~behavior_impl();

  behavior_impl(duration tout = duration{});

  virtual bhvr_invoke_result invoke(message&);

  inline bhvr_invoke_result invoke(message&& arg) {
    message tmp(std::move(arg));
    return invoke(tmp);
  }

  virtual void handle_timeout();

  inline const duration& timeout() const {
    return timeout_;
  }

  virtual pointer copy(const generic_timeout_definition& tdef) const = 0;

  pointer or_else(const pointer& other);

protected:
  duration timeout_;
  match_case_info* begin_;
  match_case_info* end_;
};

template <size_t Pos, size_t Size>
struct defaut_bhvr_impl_init {
  template <class Array, class Tuple>
  static void init(Array& arr, Tuple& tup) {
    auto& x = arr[Pos];
    x.ptr = &std::get<Pos>(tup);
    x.has_wildcard = x.ptr->has_wildcard();
    x.type_token = x.ptr->type_token();
    defaut_bhvr_impl_init<Pos + 1, Size>::init(arr, tup);
  }
};

template <size_t Size>
struct defaut_bhvr_impl_init<Size, Size> {
  template <class Array, class Tuple>
  static void init(Array&, Tuple&) {
    // nop
  }
};


template <class Tuple>
class default_behavior_impl : public behavior_impl {
public:
  static constexpr size_t num_cases = std::tuple_size<Tuple>::value;

  template <class T>
  default_behavior_impl(const T& tup) : cases_(std::move(tup)) {
    init();
  }

  template <class T, class F>
  default_behavior_impl(const T& tup, const timeout_definition<F>& d)
      : behavior_impl(d.timeout),
        cases_(tup),
        fun_(d.handler) {
    init();
  }

  typename behavior_impl::pointer
  copy(const generic_timeout_definition& tdef) const override {
    return make_counted<default_behavior_impl<Tuple>>(cases_, tdef);
  }

  void handle_timeout() override {
    fun_();
  }

private:
  void init() {
    defaut_bhvr_impl_init<0, num_cases>::init(arr_, cases_);
    begin_ = arr_.data();
    end_ = begin_ + arr_.size();
  }

  Tuple cases_;
  std::array<match_case_info, num_cases> arr_;
  std::function<void()> fun_;
};

// eor = end of recursion
// ra  = reorganize arguments

template <class R, class... Ts>
intrusive_ptr<R> make_behavior_eor(const std::tuple<Ts...>& match_cases) {
  return make_counted<R>(match_cases);
}

template <class R, class... Ts, class F>
intrusive_ptr<R> make_behavior_eor(const std::tuple<Ts...>& match_cases,
                                   const timeout_definition<F>& td) {
  return make_counted<R>(match_cases, td);
}

template <class F, bool IsMC = std::is_base_of<match_case, F>::value>
struct lift_to_mctuple {
  using type = std::tuple<trivial_match_case<F>>;
};

template <class T>
struct lift_to_mctuple<T, true> {
  using type = std::tuple<T>;
};

template <class... Ts>
struct lift_to_mctuple<std::tuple<Ts...>, false> {
  using type = std::tuple<Ts...>;
};

template <class F>
struct lift_to_mctuple<timeout_definition<F>, false> {
  using type = std::tuple<>;
};

template <class T, class... Ts>
struct join_std_tuples;

template <class T>
struct join_std_tuples<T> {
  using type = T;
};

template <class... Prefix, class... Infix, class... Suffix>
struct join_std_tuples<std::tuple<Prefix...>, std::tuple<Infix...>, Suffix...>
    : join_std_tuples<std::tuple<Prefix..., Infix...>, Suffix...> {
  // nop
};

// this function reorganizes its arguments to shift the timeout definition
// to the front (receives it at the tail)
template <class R, class F, class... Ts>
intrusive_ptr<R> make_behavior_ra(const timeout_definition<F>& td,
                                  const tail_argument_token&, const Ts&... xs) {
  return make_behavior_eor<R>(std::tuple_cat(to_match_case_tuple(xs)...), td);
}

template <class R, class... Ts>
intrusive_ptr<R> make_behavior_ra(const tail_argument_token&, const Ts&... xs) {
  return make_behavior_eor<R>(std::tuple_cat(to_match_case_tuple(xs)...));
}

// for some reason, this call is ambigious on GCC without enable_if
template <class R, class T, class... Ts>
typename std::enable_if<
  ! std::is_same<T, tail_argument_token>::value,
  intrusive_ptr<R>
>::type
make_behavior_ra(const T& x, const Ts&... xs) {
  return make_behavior_ra<R>(xs..., x);
}

// this function reorganizes its arguments to shift the timeout definition
// to the front (receives it at the tail)
template <class... Ts>
intrusive_ptr<
  default_behavior_impl<
    typename join_std_tuples<
      typename lift_to_mctuple<Ts>::type...
    >::type
  >>
make_behavior(const Ts&... xs) {
  using result_type =
    default_behavior_impl<
      typename join_std_tuples<
        typename lift_to_mctuple<Ts>::type...
      >::type
    >;
  tail_argument_token eoa;
  return make_behavior_ra<result_type>(xs..., eoa);
}

using behavior_impl_ptr = intrusive_ptr<behavior_impl>;

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BEHAVIOR_IMPL_HPP

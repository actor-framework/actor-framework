// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

// imi = interface_mismatch_implementation
// Precondition: Pos == 0 && len(Xs) == len(Ys) && len(Zs) == 0
// Iterate over Xs to find a match in Ys; Zs is used as temporary storage
// to iterate over Ys. On a match, the element is removed from Xs and Ys and
// Zs are prepended to Ys again for next iteration.
// Evaluates to:
// * len(Xs) if interface check succeeds
// * Pos on a mismatch (incremented per iteration to reflect position in Xs)
template <int Pos, class Xs, class Ys, class Zs>
struct imi;

// end of recursion: success (consumed both lists)
template <int Pos>
struct imi<Pos, type_list<>, type_list<>, type_list<>> {
  static constexpr int value = Pos;
  using xs = type_list<>;
  using ys = type_list<>;
};

// end of recursion: success (consumed both lists, except the timeout)
template <int Pos, class X>
struct imi<Pos, type_list<timeout_definition<X>>, type_list<>, type_list<>> {
  static constexpr int value = Pos + 1; // count timeout def. as consumed
  using xs = type_list<>;
  using ys = type_list<>;
};

// end of recursion: failure (consumed all Xs but not all Ys)
template <int Pos, class Yin, class Yout, class... Ys>
struct imi<Pos, type_list<>, type_list<Yout(Yin), Ys...>, type_list<>> {
  static constexpr int value = -1;
  using xs = type_list<>;
  using ys = type_list<Yout(Yin), Ys...>;
};

// end of recursion: failure (consumed all Ys but not all Xs)
template <int Pos, class Xin, class Xout, class... Xs>
struct imi<Pos, type_list<Xout(Xin), Xs...>, type_list<>, type_list<>> {
  static constexpr int value = -2;
  using xs = type_list<Xout(Xin), Xs...>;
  using ys = type_list<>;
};

// end of recursion: failure (consumed all Ys except timeout but not all Xs)
template <int Pos, class X, class Y, class... Ys>
struct imi<Pos, type_list<timeout_definition<X>>, type_list<Y, Ys...>,
           type_list<>> {
  static constexpr int value = -2;
  using xs = type_list<>;
  using ys = type_list<Y, Ys...>;
};

// case #1a: exact match
template <int Pos, class Out, class... In, class... Xs, class... Ys,
          class... Zs>
struct imi<Pos, type_list<Out(In...), Xs...>, type_list<Out(In...), Ys...>,
           type_list<Zs...>>
  : imi<Pos + 1, type_list<Xs...>, type_list<Zs..., Ys...>, type_list<>> {};

// case #2: no match at position
template <int Pos, class Xout, class... Xin, class... Xs, class Yout,
          class... Yin, class... Ys, class... Zs>
struct imi<Pos, type_list<Xout(Xin...), Xs...>, type_list<Yout(Yin...), Ys...>,
           type_list<Zs...>>
  : imi<Pos, type_list<Xout(Xin...), Xs...>, type_list<Ys...>,
        type_list<Zs..., Yout(Yin...)>> {};

// case #3: no match (error)
template <int Pos, class X, class... Xs, class... Zs>
struct imi<Pos, type_list<X, Xs...>, type_list<>, type_list<Zs...>> {
  static constexpr int value = Pos;
  using xs = type_list<X, Xs...>;
  using ys = type_list<Zs...>;
};

} // namespace caf::detail

namespace caf {

/// Scans two typed MPI lists for compatibility, returning the index of the
/// first mismatch. Returns the number of elements on a match.
/// @pre len(Found) == len(Expected)
template <class Found, class Expected>
using interface_mismatch_t
  = detail::imi<0, Found, Expected, detail::type_list<>>;

} // namespace caf

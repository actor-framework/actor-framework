// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/replies_to.hpp"

namespace caf {

/// Computes the type for f*g (actor composition).
///
/// This metaprogramming function implements the following pseudo-code (with
/// `f` and `g` modelled as pairs where the first element is the input type and
/// the second type is the output type).
///
/// ~~~
/// let composed_type f g =
///   [(fst x, snd y) | x <- g, y <- f,
///                     snd x == fst y]
/// ~~~
///
/// This class implements the list comprehension above in a single shot with
/// worst case n*m template instantiations using an inner and outer loop, where
/// n is the size of `Xs` and m the size of `Ys`. `Zs` is a helper that models
/// the "inner loop variable" for generating the cross product of `Xs` and
/// `Ys`. `Rs` collects the results.
template <class Xs, class Ys, class Zs, class Rs>
struct composed_type;

// End of outer loop over Xs.
template <class Ys, class Zs, class... Rs>
struct composed_type<detail::type_list<>, Ys, Zs, detail::type_list<Rs...>> {
  using type = typed_actor<Rs...>;
};

// End of inner loop Ys (Zs).
template <class X, class... Xs, class Ys, class Rs>
struct composed_type<detail::type_list<X, Xs...>, Ys, detail::type_list<>, Rs>
  : composed_type<detail::type_list<Xs...>, Ys, Ys, Rs> {};

// Output type matches the input type of the next actor.
template <class Out, class... In, class... Xs, class Ys, class MapsTo,
          class... Zs, class... Rs>
struct composed_type<detail::type_list<Out(In...), Xs...>, Ys,
                     detail::type_list<MapsTo(Out), Zs...>,
                     detail::type_list<Rs...>>
  : composed_type<detail::type_list<Xs...>, Ys, Ys,
                  detail::type_list<Rs..., MapsTo(In...)>> {};

// Output type matches the input type of the next actor.
template <class... Out, class... In, class... Xs, class Ys, class MapsTo,
          class... Zs, class... Rs>
struct composed_type<detail::type_list<result<Out...>(In...), Xs...>, Ys,
                     detail::type_list<MapsTo(Out...), Zs...>,
                     detail::type_list<Rs...>>
  : composed_type<detail::type_list<Xs...>, Ys, Ys,
                  detail::type_list<Rs..., MapsTo(In...)>> {};

// No match, recurse over Zs.
template <class Out, class... In, class... Xs, class Ys, class MapsTo,
          class... Unrelated, class... Zs, class Rs>
struct composed_type<detail::type_list<Out(In...), Xs...>, Ys,
                     detail::type_list<MapsTo(Unrelated...), Zs...>, Rs>
  : composed_type<detail::type_list<Out(In...), Xs...>, Ys,
                  detail::type_list<Zs...>, Rs> {};

/// Convenience type alias.
/// @relates composed_type
template <class F, class G>
using composed_type_t =
  typename composed_type<G, F, F, detail::type_list<>>::type;

} // namespace caf

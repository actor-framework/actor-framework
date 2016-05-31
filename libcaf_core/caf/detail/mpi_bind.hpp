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

#ifndef CAF_DETAIL_MPI_BIND_HPP
#define CAF_DETAIL_MPI_BIND_HPP

#include <functional>

#include "caf/none.hpp"
#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

template <class T, size_t Pos>
class mpi_bind_sig_arg_t {
  // nop
};

template <class T,
          int PlaceholderValue,
          size_t Pos,
          size_t Size,
          bool InRange = PlaceholderValue <= static_cast<int>(Size)>
struct mpi_bind_sig_single {
  using type =
    mpi_bind_sig_arg_t<
      typename detail::tl_at<T, Pos>::type,
      PlaceholderValue - 1
    >;
};

template <class T, size_t Pos, size_t Size>
struct mpi_bind_sig_single<T, 0, Pos, Size, true> {
  using type = void;
};

template <class T, int PlaceholderValue, size_t Pos, size_t Size>
struct mpi_bind_sig_single<T, PlaceholderValue, Pos, Size, false> {
  using type = none_t;
};

template <size_t I, size_t Size, class Args, class In, class... Ts>
struct mpi_bind_sig_impl {
  using sub =
    typename mpi_bind_sig_single<
      In,
      std::is_placeholder<typename detail::tl_at<Args, I>::type>::value,
      I,
      Size
    >::type;
  using type =
    typename std::conditional<
      std::is_same<sub, none_t>::value,
      void,
      typename std::conditional<
        std::is_same<sub, void>::value,
        typename mpi_bind_sig_impl<I + 1, Size, Args, In, Ts...>::type,
        typename mpi_bind_sig_impl<I + 1, Size, Args, In, Ts..., sub>::type
      >::type
    >::type;
};

template <size_t Size, class Args, class In>
struct mpi_bind_sig_impl<Size, Size, Args, In> {
  using type = void;
};

template <size_t Size, class Args, class In, class T, class... Ts>
struct mpi_bind_sig_impl<Size, Size, Args, In, T, Ts...> {
  using type = detail::type_list<T, Ts...>;
};

template <size_t I, class In, class Out, class... Ts>
struct mpi_bind_sort;

template <size_t I, class Out, class... Ts>
struct mpi_bind_sort<I, detail::type_list<>, Out, Ts...> {
  using type = typed_mpi<detail::type_list<Ts...>, Out>;
};

template <size_t I, size_t X, class Arg, class... Args, class Out, class... Ts>
struct mpi_bind_sort<I, detail::type_list<mpi_bind_sig_arg_t<Arg, X>, Args...>,
                     Out, Ts...> {
  using type =
    typename mpi_bind_sort<
      I,
      detail::type_list<Args..., mpi_bind_sig_arg_t<Arg, X>>,
      Out,
      Ts...
    >::type;
};

template <size_t I, class Arg, class... Args, class Out, class... Ts>
struct mpi_bind_sort<I, detail::type_list<mpi_bind_sig_arg_t<Arg, I>, Args...>,
                     Out, Ts...> {
  using type =
    typename mpi_bind_sort<
      I + 1,
      detail::type_list<Args...>,
      Out,
      Ts...,
      Arg
    >::type;
};

template <class Sig, class Args>
struct mpi_bind_sig {
  static constexpr size_t num_args = detail::tl_size<Args>::value;
  using type =
    typename mpi_bind_sort<
      0,
      typename mpi_bind_sig_impl<
        0,
        num_args,
        Args,
        typename Sig::input_types
      >::type,
      typename Sig::output_types
    >::type;
};

template <template <class...> class Target, class Sigs, class... Args>
struct mpi_bind;

template <template <class...> class Target, class... Sigs, class... Ts>
struct mpi_bind<Target, detail::type_list<Sigs...>, Ts...> {
  using args = detail::type_list<Ts...>;
  using bound_sigs = detail::type_list<typename mpi_bind_sig<Sigs, args>::type...>;
  // drop any mismatch (void) and rebuild typed actor handle
  using type =
    typename detail::tl_apply<
      typename detail::tl_filter_not_type<bound_sigs, void>::type,
      Target
    >::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MPI_BIND_HPP

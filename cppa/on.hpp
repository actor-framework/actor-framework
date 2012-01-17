/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef ON_HPP
#define ON_HPP

#include <chrono>
#include <memory>

#include "cppa/atom.hpp"
#include "cppa/invoke.hpp"
#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/arg_match_t.hpp"
#include "cppa/util/callable_trait.hpp"
#include "cppa/util/concat_type_lists.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/ref_counted_impl.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa { namespace detail {

class timed_invoke_rule_builder
{

    util::duration m_timeout;

 public:

    constexpr timed_invoke_rule_builder(util::duration const& d) : m_timeout(d)
    {
    }

    template<typename F>
    timed_invoke_rules operator>>(F&& f)
    {
        typedef timed_invokable_impl<F> impl;
        return timed_invokable_ptr(new impl(m_timeout, std::forward<F>(f)));
    }

};

template<typename... TypeList>
class invoke_rule_builder
{

    typedef util::arg_match_t arg_match_t;

    typedef util::type_list<TypeList...> raw_types;

    static constexpr bool is_complete =
            !std::is_same<arg_match_t, typename raw_types::back>::value;

    typedef typename util::if_else_c<is_complete == false,
                                     typename util::tl_pop_back<raw_types>::type,
                                     util::wrapped<raw_types> >::type
            types;

    static_assert(util::tl_find<types, arg_match_t>::value == -1,
                  "arg_match is allowed only as last parameter for on()");

    typedef typename util::tl_apply<types,
                                    detail::implicit_conversions>::type
            converted_types;

    typedef typename pattern_from_type_list<converted_types>::type
            pattern_type;

    std::unique_ptr<pattern_type> m_ptr;

    typedef typename pattern_type::tuple_view_type tuple_view_type;

    template<typename F>
    invoke_rules cr_rules(F&& f, std::integral_constant<bool, true>)
    {
        typedef invokable_impl<tuple_view_type, pattern_type, F> impl;
        return invokable_ptr(new impl(std::move(m_ptr),std::forward<F>(f)));
    }

    template<typename F>
    invoke_rules cr_rules(F&& f, std::integral_constant<bool, false>)
    {
        using namespace ::cppa::util;
        typedef typename get_callable_trait<F>::type ctrait;
        typedef typename ctrait::arg_types raw_types;
        static_assert(raw_types::size > 0, "functor has no arguments");
        typedef typename tl_apply<raw_types,rm_ref>::type new_types;
        typedef typename tl_concat<converted_types,new_types>::type types;
        typedef typename pattern_from_type_list<types>::type epattern;
        typedef typename epattern::tuple_view_type tuple_view_type;
        typedef invokable_impl<tuple_view_type, epattern, F> impl;
        std::unique_ptr<epattern> pptr(extend_pattern<epattern>(m_ptr.get()));
        return invokable_ptr(new impl(std::move(pptr), std::forward<F>(f)));
    }

 public:

    template<typename... Args>
    invoke_rule_builder(Args const&... args)
        : m_ptr(new pattern_type(args...))
    {
    }


    template<typename F>
    invoke_rules operator>>(F&& f)
    {
        std::integral_constant<bool,is_complete> token;
        return cr_rules(std::forward<F>(f), token);
    }

};

class on_the_fly_invoke_rule_builder
{

 public:

    constexpr on_the_fly_invoke_rule_builder()
    {
    }

    template<typename F>
    invoke_rules operator>>(F&& f) const
    {
        using namespace ::cppa::util;
        typedef typename get_callable_trait<F>::type ctrait;
        typedef typename ctrait::arg_types raw_types;
        static_assert(raw_types::size > 0, "functor has no arguments");
        typedef typename tl_apply<raw_types,rm_ref>::type types;
        typedef typename pattern_from_type_list<types>::type pattern_type;
        typedef typename pattern_type::tuple_view_type tuple_view_type;
        typedef invokable_impl<tuple_view_type, pattern_type, F> impl;
        std::unique_ptr<pattern_type> pptr(new pattern_type);
        return invokable_ptr(new impl(std::move(pptr), std::forward<F>(f)));
    }

};

} } // cppa::detail

namespace cppa {

template<typename T>
constexpr typename detail::boxed<T>::type val()
{
    return typename detail::boxed<T>::type();
}

constexpr anything any_vals = anything();

typedef typename detail::boxed<util::arg_match_t>::type boxed_arg_match_t;

constexpr boxed_arg_match_t arg_match = boxed_arg_match_t();

constexpr detail::on_the_fly_invoke_rule_builder on_arg_match;

template<typename Arg0, typename... Args>
detail::invoke_rule_builder<typename detail::unboxed<Arg0>::type,
                            typename detail::unboxed<Args>::type...>
on(Arg0 const& arg0, Args const&... args)
{
    return { arg0, args... };
}

template<typename... TypeList>
detail::invoke_rule_builder<TypeList...> on()
{
    return { };
}

template<atom_value A0, typename... TypeList>
detail::invoke_rule_builder<atom_value, TypeList...> on()
{
    return { A0 };
}

template<atom_value A0, atom_value A1, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, TypeList...> on()
{
    return { A0, A1 };
}

template<atom_value A0, atom_value A1, atom_value A2, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return { A0, A1, A2 };
}

template<atom_value A0, atom_value A1,
         atom_value A2, atom_value A3,
         typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return { A0, A1, A2, A3 };
}

template<class Rep, class Period>
constexpr detail::timed_invoke_rule_builder
after(const std::chrono::duration<Rep, Period>& d)
{
    return { util::duration(d) };
}

inline detail::invoke_rule_builder<anything> others()
{
    return { };
}

} // namespace cppa

#endif // ON_HPP

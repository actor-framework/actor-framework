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
#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
#include "cppa/behavior.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/guard_expr.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/arg_match_t.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/ref_counted_impl.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa { namespace detail {

class behavior_rvalue_builder
{

    util::duration m_timeout;

 public:

    constexpr behavior_rvalue_builder(util::duration const& d) : m_timeout(d)
    {
    }

    template<typename F>
    behavior operator>>(F&& f)
    {
        return {m_timeout, std::function<void()>{std::forward<F>(f)}};
    }

};

template<typename... TypeList>
class rvalue_builder
{

    typedef util::arg_match_t arg_match_t;

    typedef util::type_list<
                typename detail::implicit_conversions<TypeList>::type...>
            raw_types;

    static constexpr bool is_complete =
            !std::is_same<arg_match_t, typename raw_types::back>::value;

    typedef typename util::if_else_c<
                        is_complete == false,
                        typename util::tl_pop_back<raw_types>::type,
                        util::wrapped<raw_types> >::type
            types;

    static_assert(util::tl_find<types, arg_match_t>::value == -1,
                  "arg_match is allowed only as last parameter for on()");

    typedef typename pattern_from_type_list<types>::type pattern_type;

    typedef std::unique_ptr<value_matcher> vm_ptr;

    vm_ptr m_vm;

    template<typename F>
    partial_function cr_rvalue(F&& f, std::integral_constant<bool, true>)
    {
        return get_invokable_impl<pattern_type>(std::forward<F>(f),
                                                std::move(m_vm));
    }

    template<typename F>
    partial_function cr_rvalue(F&& f, std::integral_constant<bool, false>)
    {
        using namespace ::cppa::util;
        typedef typename get_callable_trait<F>::type ctrait;
        typedef typename ctrait::arg_types raw_arg_types;
        static_assert(raw_types::size > 0, "functor has no arguments");
        typedef typename tl_apply<raw_arg_types,rm_ref>::type arg_types;
        typedef typename tl_concat<types,arg_types>::type full_types;
        typedef typename pattern_from_type_list<full_types>::type epattern;
        return get_invokable_impl<epattern>(std::forward<F>(f),
                                            std::move(m_vm));
    }

 public:

    //template<operator_id OP, typename F, typename S>
    //rvalue_builder& when(guard_expr<OP, F, S> ge)
    template<typename Fun>
    rvalue_builder& when(Fun&& fun)
    {
        //TODO: static_assert
        typedef typename util::rm_ref<Fun>::type fun_type;
        if (m_vm)
        {
            struct impl1 : value_matcher
            {
                vm_ptr m_ptr;
                fun_type m_fun;
                impl1(vm_ptr&& ptr, Fun&& f)
                    : m_ptr(std::move(ptr)), m_fun(std::forward<Fun>(f))
                {
                }
                bool operator()(any_tuple const& tup) const
                {
                    if ((*m_ptr)(tup))
                    {
                        auto ttup = tuple_cast(tup, types{});
                        if (ttup)
                        {
                            return util::unchecked_apply_tuple<bool>(m_fun,
                                                                     *ttup);
                        }
                    }
                    return false;
                }
            };
            m_vm.reset(new impl1{std::move(m_vm), std::forward<Fun>(fun)});
        }
        else
        {
            struct impl2 : value_matcher
            {
                fun_type m_fun;
                impl2(Fun&& f) : m_fun(std::forward<Fun>(f))
                {
                }
                bool operator()(any_tuple const& tup) const
                {
                    auto ttup = tuple_cast(tup, types{});
                    if (ttup)
                    {
                        return util::unchecked_apply_tuple<bool>(m_fun, *ttup);
                    }
                    return false;
                }
            };
            m_vm.reset(new impl2{std::forward<Fun>(fun)});
        }
        return *this;
    }

    template<typename... Args>
    rvalue_builder(Args&&... args)
        : m_vm(pattern_type::get_value_matcher(std::forward<Args>(args)...))
    {
    }

    template<typename F>
    partial_function operator>>(F&& f)
    {
        std::integral_constant<bool, is_complete> token;
        return cr_rvalue(std::forward<F>(f), token);
    }

};

template<>
class rvalue_builder<anything>
{

    typedef pattern<anything> pattern_type;

 public:

    constexpr rvalue_builder() { }

    template<typename F>
    partial_function operator>>(F&& f)
    {
        return get_invokable_impl<pattern_type>(std::forward<F>(f));
    }

};

class on_the_fly_rvalue_builder
{

 public:

    constexpr on_the_fly_rvalue_builder()
    {
    }

    template<typename F>
    partial_function operator>>(F&& f) const
    {
        using namespace ::cppa::util;
        typedef typename get_callable_trait<F>::type ctrait;
        typedef typename ctrait::arg_types raw_types;
        static_assert(raw_types::size > 0, "functor has no arguments");
        typedef typename tl_apply<raw_types,rm_ref>::type types;
        typedef typename pattern_from_type_list<types>::type pattern_type;
        return get_invokable_impl<pattern_type>(std::forward<F>(f));
    }

};

} } // cppa::detail

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief A wildcard that matches the argument types
*         of a given callback. Must be the last argument to {@link on()}.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
constexpr ___ arg_match;

/**
 * @brief Left-hand side of a partial function expression.
 *
 * Equal to <tt>on(arg_match)</tt>.
 */
constexpr ___ on_arg_match;

/**
 * @brief A wildcard that matches any value of type @p T.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
template<typename T>
___ val();

/**
 * @brief A wildcard that matches any number of any values.
 */
constexpr anything any_vals;


/**
 * @brief Left-hand side of a partial function expression that matches values.
 *
 * This overload can be used with the wildcards {@link cppa::val val},
 * {@link cppa::any_vals any_vals} and {@link cppa::arg_match arg_match}.
 */
template<typename Arg0, typename... Args>
___ on(Arg0 const& arg0, Args const&... args);

/**
 * @brief Left-hand side of a partial function expression that matches types.
 *
 * This overload matches types only. The type {@link cppa::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template<typename... Ts>
___ on();

/**
 * @brief Left-hand side of a partial function expression that matches types.
 *
 * This overload matches up to four leading atoms.
 * The type {@link cppa::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template<atom_value... Atoms, typename... Ts>
___ on();

#else

template<typename T>
constexpr typename detail::boxed<T>::type val()
{
    return typename detail::boxed<T>::type();
}

constexpr anything any_vals = anything();

typedef typename detail::boxed<util::arg_match_t>::type boxed_arg_match_t;

constexpr boxed_arg_match_t arg_match = boxed_arg_match_t();

constexpr detail::on_the_fly_rvalue_builder on_arg_match;

template<typename Arg0, typename... Args>
detail::rvalue_builder<typename detail::unboxed<typename util::rm_ref<Arg0>::type>::type,
                       typename detail::unboxed<typename util::rm_ref<Args>::type>::type...>
on(Arg0&& arg0, Args&&... args)
{
    return {std::forward<Arg0>(arg0), std::forward<Args>(args)...};
}

template<typename... TypeList>
detail::rvalue_builder<TypeList...> on()
{
    return { };
}

template<atom_value A0, typename... Ts>
detail::rvalue_builder<atom_value, Ts...> on()
{
    return { A0 };
}

template<atom_value A0, atom_value A1, typename... Ts>
detail::rvalue_builder<atom_value, atom_value, Ts...> on()
{
    return { A0, A1 };
}

template<atom_value A0, atom_value A1, atom_value A2, typename... Ts>
detail::rvalue_builder<atom_value, atom_value, atom_value, Ts...> on()
{
    return { A0, A1, A2 };
}

template<atom_value A0, atom_value A1,
         atom_value A2, atom_value A3,
         typename... Ts>
detail::rvalue_builder<atom_value, atom_value, atom_value,
                       atom_value, Ts...> on()
{
    return { A0, A1, A2, A3 };
}

template<class Rep, class Period>
constexpr detail::behavior_rvalue_builder
after(std::chrono::duration<Rep, Period> const& d)
{
    return { util::duration(d) };
}

inline detail::rvalue_builder<anything> others()
{
    return { };
}

#endif

} // namespace cppa

#endif // ON_HPP

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


#ifndef INVOKABLE_HPP
#define INVOKABLE_HPP

#include <vector>
#include <memory>
#include <cstddef>
#include <cstdint>

#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/matches.hpp"

namespace cppa { namespace detail {

class invokable
{

    invokable(invokable const&) = delete;
    invokable& operator=(invokable const&) = delete;

 public:

    invokable* next;

    inline invokable() : next(nullptr) { }
    virtual ~invokable();

    // Checks whether the types of @p value match the pattern.
    virtual bool types_match(any_tuple const& value) const;
    // Checks whether this invokable could be invoked with @p value.
    virtual bool could_invoke(any_tuple const& value) const;

    // Type checking.
    virtual bool invoke(any_tuple& value) const;
    // Suppress type checking.
    virtual bool unsafe_invoke(any_tuple& value) const;

};

enum mapping_policy
{
    do_not_map,
    map_to_bool,
    map_to_option
};

template<mapping_policy, class Pattern> // do_not_map
struct pattern_policy
{
    Pattern m_pattern;
    template<typename... Args>
    pattern_policy(Args&&... args) : m_pattern(std::forward<Args>(args)...) { }
    bool types_match(any_tuple const& value) const
    {
        return matches_types(value, m_pattern);
    }
    bool could_invoke(any_tuple const& value) const
    {
        return matches(value, m_pattern);
    }
    template<typename Fun>
    bool call(Fun const& fun, any_tuple& value) const
    {
        fun(value);
        return true;
    }
    template<typename Fun>
    bool call_unsafe(Fun const& fun, any_tuple& value) const
    {
        fun(value);
        return true;
    }
};

template<class Pattern>
struct pattern_policy<map_to_option, Pattern>
{
    Pattern m_pattern;
    template<typename... Args>
    pattern_policy(Args&&... args) : m_pattern(std::forward<Args>(args)...) { }
    bool types_match(any_tuple const& value) const
    {
        return matches_types(value, m_pattern);
    }
    bool could_invoke(any_tuple const& value) const
    {
        return matches(value, m_pattern);
    }
    template<typename Fun>
    bool call(Fun const& fun, any_tuple& value) const
    {
        auto result = moving_tuple_cast(value, m_pattern);
        if (result)
        {
            util::apply_tuple(fun, *result);
            return true;
        }
        return false;
    }
    template<typename Fun>
    bool call_unsafe(Fun const& fun, any_tuple& value) const
    {
        auto result = unsafe_tuple_cast(value, m_pattern);
        if (result)
        {
            util::apply_tuple(fun, *result);
            return true;
        }
        return false;
    }
};

template<class Pattern>
struct pattern_policy<map_to_bool, Pattern>
{
    Pattern m_pattern;
    template<typename... Args>
    pattern_policy(Args&&... args) : m_pattern(std::forward<Args>(args)...) { }
    bool types_match(any_tuple const& value) const
    {
        return matches_types(value, m_pattern);
    }
    bool could_invoke(any_tuple const& value) const
    {
        return matches(value, m_pattern);
    }
    template<typename Fun>
    bool call(Fun const& fun, any_tuple& value) const
    {
        if (could_invoke(value))
        {
            fun();
            return true;
        }
        return false;
    }
    template<typename Fun>
    bool call_unsafe(Fun const& fun, any_tuple& value) const
    {
        if (could_invoke(value))
        {
            fun();
            return true;
        }
        return false;
    }
};

struct dummy_policy
{
    inline bool types_match(any_tuple const&) const
    {
        return true;
    }
    inline bool could_invoke(any_tuple const&) const
    {
        return true;
    }
    template<typename Fun>
    inline bool call(Fun const& fun, any_tuple&) const
    {
        fun();
        return true;
    }
    template<typename Fun>
    inline bool call_unsafe(Fun const& fun, any_tuple&) const
    {
        fun();
        return true;
    }
};

template<typename Fun, class Policy>
struct invokable_impl : public invokable
{

    Fun m_fun;
    Policy m_policy;

    template<typename Arg0, typename... Args>
    invokable_impl(Arg0&& arg0, Args&&... args)
        : m_fun(std::forward<Arg0>(arg0))
        , m_policy(std::forward<Args>(args)...)
    {
    }

    bool invoke(any_tuple& value) const
    {
        return m_policy.call(m_fun, value);
    }

    bool unsafe_invoke(any_tuple& value) const
    {
        return m_policy.call_unsafe(m_fun, value);
    }

    bool types_match(any_tuple const& value) const
    {
        return m_policy.types_match(value);
    }

    bool could_invoke(any_tuple const& value) const
    {
        return m_policy.could_invoke(value);
    }

};

template<class ArgTypes>
constexpr mapping_policy get_mapping_policy()
{
    return (ArgTypes::size == 0)
            ? map_to_bool
            : ((   std::is_same<typename ArgTypes::head, any_tuple>::value
                && ArgTypes::size == 1)
               ? do_not_map
               : map_to_option);
}

template<class Container>
struct filtered;

template<typename... Ts>
struct filtered<util::type_list<Ts...> >
{
    typedef typename util::tl_filter_not<util::type_list<Ts...>, is_anything>::type
            types;
};

template<typename... Ts>
struct filtered<pattern<Ts...> >
{
    typedef typename filtered<util::type_list<Ts...>>::types types;
};


template<typename Fun, class Pattern>
struct select_invokable_impl
{
    typedef typename util::get_arg_types<Fun>::types qualified_arg_types;
    typedef typename util::tl_map<qualified_arg_types, util::rm_ref>::type
            arg_types;
    static constexpr mapping_policy mp = get_mapping_policy<arg_types>();
    typedef invokable_impl<Fun, pattern_policy<mp, Pattern> > type;
};

template<typename Fun>
struct select_invokable_impl<Fun, pattern<anything> >
{
    typedef typename util::get_arg_types<Fun>::types qualified_arg_types;
    typedef typename util::tl_map<qualified_arg_types, util::rm_ref>::type
            arg_types;
    typedef typename util::rm_ref<typename arg_types::head>::type arg0;
    static_assert(arg_types::size < 2, "functor has too many arguments");
    static_assert(   arg_types::size == 0
                  || std::is_same<any_tuple, arg0>::value,
                  "bad signature");
    typedef invokable_impl<Fun, dummy_policy> type;
};

template<class Pattern, typename Fun>
std::unique_ptr<invokable> get_invokable_impl(Fun&& fun,
                                              std::unique_ptr<value_matcher>&& vm)
{
    typedef std::unique_ptr<invokable> result;
    if (vm)
    {
        typedef typename select_invokable_impl<Fun, Pattern>::type impl1;
        return result{new impl1{std::forward<Fun>(fun), std::move(vm)}};
    }
    typedef typename Pattern::types pattern_types;
    typedef typename select_invokable_impl<Fun, pattern_types>::type impl2;
    return result{new impl2{std::forward<Fun>(fun)}};
}

template<class Pattern, typename Fun>
std::unique_ptr<invokable> get_invokable_impl(Fun&& fun)
{
    typedef typename Pattern::types pattern_types;
    typedef typename select_invokable_impl<Fun, pattern_types>::type impl;
    return std::unique_ptr<invokable>{new impl(std::forward<Fun>(fun))};
}

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

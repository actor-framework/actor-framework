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
#include <cstddef>
#include <cstdint>

#include "cppa/any_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/any_tuple_view.hpp"
#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/intermediate.hpp"

// forward declaration
namespace cppa { class any_tuple; }

namespace cppa { namespace detail {

class invokable_base
{

    invokable_base(invokable_base const&) = delete;
    invokable_base& operator=(invokable_base const&) = delete;

 public:

    invokable_base() = default;
    virtual ~invokable_base();
    virtual bool invoke(any_tuple const&) const = 0;
    virtual bool invoke(any_tuple_view const&) const = 0;

};

class timed_invokable : public invokable_base
{

    util::duration m_timeout;

 protected:

    timed_invokable(util::duration const&);

 public:

    inline util::duration const& timeout() const
    {
        return m_timeout;
    }

};

template<class TargetFun>
class timed_invokable_impl : public timed_invokable
{

    typedef timed_invokable super;

    TargetFun m_target;

 public:

    template<typename F>
    timed_invokable_impl(util::duration const& d, F&& fun)
        : super(d), m_target(std::forward<F>(fun))
    {
    }

    bool invoke(any_tuple const&) const
    {
        m_target();
        return true;
    }

    bool invoke(any_tuple_view const&) const
    {
        m_target();
        return true;
    }


};

class invokable : public invokable_base
{

 public:

    virtual intermediate* get_intermediate(any_tuple const&) = 0;

};

template<size_t NumArgs, typename Fun, class Tuple, class Pattern>
class invokable_impl : public invokable
{

    struct iimpl : intermediate
    {
        Fun m_fun;
        Tuple m_args;
        template<typename F> iimpl(F&& fun) : m_fun(std::forward<F>(fun)) { }
        void invoke() { util::apply_tuple(m_fun, m_args); }
    }
    m_iimpl;
    std::unique_ptr<Pattern> m_pattern;

    template<typename T>
    bool invoke_impl(T const& data) const
    {
        auto tuple_option = tuple_cast(data, *m_pattern);
        if (tuple_option.valid())
        {
            util::apply_tuple(m_iimpl.m_fun, *tuple_option);
            return true;
        }
        return false;
    }

 public:

    template<typename F>
    invokable_impl(F&& fun, std::unique_ptr<Pattern>&& pptr)
        : m_iimpl(std::forward<F>(fun)), m_pattern(std::move(pptr))
    {
    }

    bool invoke(any_tuple const& data) const
    {
        return invoke_impl(data);
    }

    bool invoke(any_tuple_view const& data) const
    {
        return invoke_impl(data);
    }

    intermediate* get_intermediate(any_tuple const& data)
    {
        auto tuple_option = tuple_cast(data, *m_pattern);
        if (tuple_option.valid())
        {
            m_iimpl.m_args = std::move(*tuple_option);
            return &m_iimpl;
        }
        return nullptr;
    }

};

template<typename Fun, class Tuple, class Pattern>
class invokable_impl<0, Fun, Tuple, Pattern> : public invokable
{

    struct iimpl : intermediate
    {
        Fun m_fun;
        template<typename F> iimpl(F&& fun) : m_fun(std::forward<F>(fun)) { }
        void invoke() { m_fun(); }
    }
    m_iimpl;
    std::unique_ptr<Pattern> m_pattern;

    template<typename T>
    bool invoke_impl(T const& data) const
    {
        if (matches(data, *m_pattern))
        {
            m_iimpl.m_fun();
            return true;
        }
        return false;
    }

 public:

    template<typename F>
    invokable_impl(F&& fun, std::unique_ptr<Pattern>&& pptr)
        : m_iimpl(std::forward<F>(fun)), m_pattern(std::move(pptr))
    {
    }

    bool invoke(any_tuple const& data) const
    {
        return invoke_impl(data);
    }

    bool invoke(any_tuple_view const& data) const
    {
        return invoke_impl(data);
    }

    intermediate* get_intermediate(any_tuple const& data)
    {
        return matches(data, *m_pattern) ? &m_iimpl : nullptr;
    }

};

template<typename Fun, class Pattern>
struct select_invokable_impl
{
    typedef typename util::get_arg_types<Fun>::types arg_types;
    typedef typename Pattern::filtered_types filtered_types;
    typedef typename tuple_from_type_list<filtered_types>::type tuple_type;
    typedef invokable_impl<arg_types::size, Fun, tuple_type, Pattern> type;
};

template<typename Fun, class Pattern>
std::unique_ptr<invokable> get_invokable_impl(Fun&& fun,
                                              std::unique_ptr<Pattern>&& pptr)
{
    typedef typename select_invokable_impl<Fun, Pattern>::type result;
    return std::unique_ptr<invokable>(new result(std::forward<Fun>(fun),
                                                 std::move(pptr)));
}

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

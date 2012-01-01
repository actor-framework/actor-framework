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

#include "cppa/invoke.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/fixed_vector.hpp"
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

    timed_invokable_impl(util::duration const& d, TargetFun const& tfun)
        : super(d), m_target(tfun)
    {
    }

    bool invoke(any_tuple const&) const
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

template<class TupleView, class Pattern, class TargetFun>
class invokable_impl : public invokable
{

    struct iimpl : intermediate
    {

        TargetFun const& m_target;
        TupleView m_args;

        iimpl(TargetFun const& tf) : m_target(tf)
        {
        }

        void invoke() // override
        {
            cppa::invoke(m_target, m_args);
        }

    };

    typedef util::fixed_vector<size_t, TupleView::num_elements> vector_type;

    std::unique_ptr<Pattern> m_pattern;
    TargetFun m_target;
    iimpl m_iimpl;

 public:

    invokable_impl(std::unique_ptr<Pattern>&& pptr, TargetFun const& mt)
        : m_pattern(std::move(pptr)), m_target(mt), m_iimpl(m_target)
    {
    }

    bool invoke(any_tuple const& data) const
    {

        vector_type mv;
        if ((*m_pattern)(data, &mv))
        {
            if (mv.size() == data.size())
            {
                // "perfect" match; no mapping needed at all
                TupleView tv = TupleView::from(data.vals());
                cppa::invoke(m_target, tv);
            }
            else
            {
                // mapping needed
                TupleView tv(data.vals(), mv);
                cppa::invoke(m_target, tv);
            }
            return true;
        }
        return false;
    }

    intermediate* get_intermediate(any_tuple const& data)
    {
        vector_type mv;
        if ((*m_pattern)(data, &mv))
        {
            if (mv.size() == data.size())
            {
                // perfect match
                m_iimpl.m_args = TupleView::from(data.vals());
            }
            else
            {
                m_iimpl.m_args = TupleView(data.vals(), mv);
            }
            return &m_iimpl;
        }
        return nullptr;
    }

};

template<template<class...> class TupleClass, class Pattern, class TargetFun>
class invokable_impl<TupleClass<>, Pattern, TargetFun> : public invokable
{

    struct iimpl : intermediate
    {
        TargetFun const& m_target;

        iimpl(TargetFun const& tf) : m_target(tf)
        {
        }

        void invoke()
        {
            m_target();
        }
    };

    std::unique_ptr<Pattern> m_pattern;
    TargetFun m_target;
    iimpl m_iimpl;

 public:

    invokable_impl(std::unique_ptr<Pattern>&& pptr, TargetFun const& mt)
        : m_pattern(std::move(pptr)), m_target(mt), m_iimpl(m_target)
    {
    }

    bool invoke(any_tuple const& data) const
    {
        if ((*m_pattern)(data, nullptr))
        {
            m_target();
            return true;
        }
        return false;
    }

    intermediate* get_intermediate(any_tuple const& data)
    {
        return ((*m_pattern)(data, nullptr)) ? &m_iimpl : nullptr;
    }

};

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

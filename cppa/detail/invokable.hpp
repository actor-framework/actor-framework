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

#include "cppa/match.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/intermediate.hpp"

// forward declaration
namespace cppa { class any_tuple; class partial_function; }

namespace cppa { namespace detail {

class invokable
{

    invokable(invokable const&) = delete;
    invokable& operator=(invokable const&) = delete;

 public:

    invokable() = default;
    virtual ~invokable();

    virtual bool invoke(any_tuple const&) const = 0;
    // Suppress type checking.
    virtual bool unsafe_invoke(any_tuple const& value) const = 0;
    // Checks whether the types of @p value match the pattern.
    virtual bool types_match(any_tuple const& value) const = 0;
    // Checks whether this invokable could be invoked with @p value.
    virtual bool could_invoke(any_tuple const& value) const = 0;
    // Prepare this invokable.
    virtual intermediate* get_intermediate(any_tuple const& value) = 0;
    // Prepare this invokable and suppress type checking.
    virtual intermediate* get_unsafe_intermediate(any_tuple const& value) = 0;

};

template<class Tuple, class Pattern>
struct abstract_invokable : public invokable
{
    std::unique_ptr<Pattern> m_pattern;
    abstract_invokable(std::unique_ptr<Pattern>&& pptr) : m_pattern(std::move(pptr))
    {
    }
    bool types_match(any_tuple const& value) const
    {
        return match_types(value, *m_pattern);
    }
    bool could_invoke(any_tuple const& value) const
    {
        return match(value, *m_pattern);
    }
};

template<bool CheckValues, size_t NumArgs, typename Fun, class Tuple, class Pattern>
class invokable_impl : public abstract_invokable<Tuple, Pattern>
{

    typedef abstract_invokable<Tuple, Pattern> super;

    template<typename T>
    inline bool invoke_impl(option<T>&& tup) const
    {
        if (tup)
        {
            util::apply_tuple(m_iimpl.m_fun, *tup);
            return true;
        }
        return false;
    }

    template<typename T>
    inline intermediate* get_intermediate_impl(option<T>&& tup)
    {
        if (tup)
        {
            m_iimpl.m_args = std::move(*tup);
            return &m_iimpl;
        }
        return nullptr;
    }

 protected:

    struct iimpl : intermediate
    {
        Fun m_fun;
        Tuple m_args;
        template<typename F> iimpl(F&& fun) : m_fun(std::forward<F>(fun)) { }
        void invoke() { util::apply_tuple(m_fun, m_args); }
    }
    m_iimpl;

 public:

    template<typename F>
    invokable_impl(F&& fun, std::unique_ptr<Pattern>&& pptr)
        : super(std::move(pptr)),  m_iimpl(std::forward<F>(fun))
    {
    }

    bool invoke(any_tuple const& value) const
    {
        return invoke_impl(tuple_cast(value, *(this->m_pattern)));
    }

    bool unsafe_invoke(any_tuple const& value) const
    {
        return invoke_impl(unsafe_tuple_cast(value, *(this->m_pattern)));
    }

    intermediate* get_intermediate(any_tuple const& value)
    {
        return get_intermediate_impl(tuple_cast(value, *(this->m_pattern)));
    }

    intermediate* get_unsafe_intermediate(any_tuple const& value)
    {
        return get_intermediate_impl(unsafe_tuple_cast(value, *(this->m_pattern)));
    }

};

template<size_t NumArgs, typename Fun, class Tuple, class Pattern>
struct invokable_impl<false, NumArgs, Fun, Tuple, Pattern> : public invokable_impl<true, NumArgs, Fun, Tuple, Pattern>
{
    typedef invokable_impl<true, NumArgs, Fun, Tuple, Pattern> super;
    template<typename F>
    invokable_impl(F&& fun, std::unique_ptr<Pattern>&& pptr)
        : super(std::forward<F>(fun), std::move(pptr))
    {
    }
    bool unsafe_invoke(any_tuple const& value) const
    {
        auto tup = forced_tuple_cast(value, *(this->m_pattern));
        util::apply_tuple((this->m_iimpl).m_fun, tup);
        return true;
    }
    intermediate* get_unsafe_intermediate(any_tuple const& value)
    {
        (this->m_iimpl).m_args = forced_tuple_cast(value, *(this->m_pattern));
        return &(this->m_iimpl);
    }
};

template<typename Fun, class Tuple, class Pattern>
class invokable_impl<true, 0, Fun, Tuple, Pattern> : public abstract_invokable<Tuple, Pattern>
{

    typedef abstract_invokable<Tuple, Pattern> super;

    template<typename... P>
    inline bool unsafe_vmatch(any_tuple const& t, pattern<P...> const& p) const
    {
        return matcher<pattern<P...>::wildcard_pos, P...>::vmatch(t, p);
    }

 protected:

    struct iimpl : intermediate
    {
        Fun m_fun;
        template<typename F> iimpl(F&& fun) : m_fun(std::forward<F>(fun)) { }
        void invoke() { m_fun(); }
        inline bool operator()() const { m_fun(); return true; }
    }
    m_iimpl;

 public:

    template<typename F>
    invokable_impl(F&& fun, std::unique_ptr<Pattern>&& pptr)
        : super(std::move(pptr)), m_iimpl(std::forward<F>(fun))
    {
    }
    bool invoke(any_tuple const& data) const
    {
        return match(data, *(this->m_pattern)) ? m_iimpl() : false;
    }
    bool unsafe_invoke(any_tuple const& value) const
    {
        return unsafe_vmatch(value, *(this->m_pattern)) ? m_iimpl() : false;
    }
    intermediate* get_intermediate(any_tuple const& value)
    {
        return match(value, *(this->m_pattern)) ? &m_iimpl : nullptr;
    }
    intermediate* get_unsafe_intermediate(any_tuple const& value)
    {
        return unsafe_vmatch(value, *(this->m_pattern)) ? &m_iimpl : nullptr;
    }
};

template<typename Fun, class Tuple, class Pattern>
struct invokable_impl<false, 0, Fun, Tuple, Pattern> : public invokable_impl<true, 0, Fun, Tuple, Pattern>
{
    typedef invokable_impl<true, 0, Fun, Tuple, Pattern> super;
    template<typename... Args>
    invokable_impl(Args&&... args) : super(std::forward<Args>(args)...) { }
    bool unsafe_invoke(any_tuple const&) const
    {
        return (this->m_iimpl)();
    }
    intermediate* get_unsafe_intermediate(any_tuple const&)
    {
        return &(this->m_iimpl);
    }
};

template<typename Fun, class Pattern>
struct select_invokable_impl
{
    typedef typename util::get_arg_types<Fun>::types arg_types;
    typedef typename Pattern::filtered_types filtered_types;
    typedef typename tuple_from_type_list<filtered_types>::type tuple_type;
    typedef invokable_impl<true, arg_types::size, Fun, tuple_type, Pattern> type1;
    typedef invokable_impl<false, arg_types::size, Fun, tuple_type, Pattern> type2;
};

template<typename Fun, class Pattern>
std::unique_ptr<invokable> get_invokable_impl(Fun&& fun,
                                              std::unique_ptr<Pattern>&& pptr)
{
    typedef select_invokable_impl<Fun, Pattern> result;
    bool check_values = pptr->has_values();
    return std::unique_ptr<invokable>(
                check_values ? new typename result::type1(std::forward<Fun>(fun),
                                                          std::move(pptr))
                             : new typename result::type2(std::forward<Fun>(fun),
                                                          std::move(pptr)));
}

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

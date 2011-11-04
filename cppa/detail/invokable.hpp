#ifndef INVOKABLE_HPP
#define INVOKABLE_HPP

#include <vector>
#include <cstddef>
#include <cstdint>

#include "cppa/invoke.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/detail/intermediate.hpp"

// forward declaration
namespace cppa { class any_tuple; }

namespace cppa { namespace detail {

class invokable_base
{

    invokable_base(const invokable_base&) = delete;
    invokable_base& operator=(const invokable_base&) = delete;

 public:

    invokable_base() = default;
    virtual ~invokable_base();
    virtual bool invoke(const any_tuple&) const = 0;

};

class timed_invokable : public invokable_base
{

    util::duration m_timeout;

 protected:

    timed_invokable(const util::duration&);

 public:

    inline const util::duration& timeout() const
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

    timed_invokable_impl(const util::duration& d, const TargetFun& tfun)
        : super(d), m_target(tfun)
    {
    }

    bool invoke(const any_tuple&) const
    {
        m_target();
        return true;
    }

};

class invokable : public invokable_base
{

 public:

    virtual intermediate* get_intermediate(const any_tuple&) = 0;

};

template<class TupleView, class Pattern, class TargetFun>
class invokable_impl : public invokable
{

    struct iimpl : intermediate
    {

        const TargetFun& m_target;
        TupleView m_args;

        iimpl(const TargetFun& tf) : m_target(tf)
        {
        }

        void invoke() // override
        {
            cppa::invoke(m_target, m_args);
        }

    };

    std::unique_ptr<Pattern> m_pattern;
    TargetFun m_target;
    iimpl m_iimpl;

 public:

    invokable_impl(std::unique_ptr<Pattern>&& pptr, const TargetFun& mt)
        : m_pattern(std::move(pptr)), m_target(mt), m_iimpl(m_target)
    {
    }

    bool invoke(const any_tuple& data) const
    {
        std::vector<size_t> mv;
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
                TupleView tv(data.vals(), std::move(mv));
                cppa::invoke(m_target, tv);
            }
            return true;
        }
        return false;
    }

    intermediate* get_intermediate(const any_tuple& data)
    {
        std::vector<size_t> mv;
        if ((*m_pattern)(data, &mv))
        {
            if (mv.size() == data.size())
            {
                // perfect match
                m_iimpl.m_args = TupleView::from(data.vals());
            }
            else
            {
                m_iimpl.m_args = TupleView(data.vals(), std::move(mv));
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
        const TargetFun& m_target;

        iimpl(const TargetFun& tf) : m_target(tf)
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

    invokable_impl(std::unique_ptr<Pattern>&& pptr, const TargetFun& mt)
        : m_pattern(std::move(pptr)), m_target(mt), m_iimpl(m_target)
    {
    }

    bool invoke(const any_tuple& data) const
    {
        if ((*m_pattern)(data, nullptr))
        {
            m_target();
            return true;
        }
        return false;
    }

    intermediate* get_intermediate(const any_tuple& data)
    {
        return ((*m_pattern)(data, nullptr)) ? &m_iimpl : nullptr;
    }

};

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

#ifndef INTERMEDIATE_HPP
#define INTERMEDIATE_HPP

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa { namespace detail {

// intermediate is NOT thread safe
class intermediate : public ref_counted_impl<size_t>
{

    intermediate(const intermediate&) = delete;
    intermediate& operator=(const intermediate&) = delete;

 public:

    intermediate() = default;
    virtual void invoke() = 0;

};

template<typename Impl, typename View = void>
class intermediate_impl : public intermediate
{

    Impl m_impl;
    View m_view;

 public:

    intermediate_impl(const Impl& impl, const View& view)
        : intermediate(), m_impl(impl), m_view(view)
    {
    }

    virtual void invoke()
    {
        m_impl(m_view);
    }

};

template<typename Impl>
class intermediate_impl<Impl, void> : public intermediate
{

    Impl m_impl;

 public:

    intermediate_impl(const Impl& impl) : m_impl(impl) { }

    virtual void invoke()
    {
        m_impl();
    }

};

} } // namespace cppa::detail

#endif // INTERMEDIATE_HPP

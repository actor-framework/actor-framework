#ifndef INTERMEDIATE_HPP
#define INTERMEDIATE_HPP

#include <utility>

namespace cppa { namespace detail {

class intermediate
{

    intermediate(const intermediate&) = delete;
    intermediate& operator=(const intermediate&) = delete;

 public:

    intermediate() = default;

    virtual ~intermediate();

    virtual void invoke() = 0;

};

template<typename Impl, typename View = void>
class intermediate_impl : public intermediate
{

    Impl m_impl;
    View m_view;

 public:

    template<typename Arg0, typename Arg1>
    intermediate_impl(Arg0&& impl, Arg1&& view)
        : intermediate()
        , m_impl(std::forward<Arg0>(impl))
        , m_view(std::forward<Arg1>(view))
    {
    }

    virtual void invoke() /*override*/
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

    intermediate_impl(Impl&& impl) : m_impl(std::move(impl)) { }

    virtual void invoke() /*override*/
    {
        m_impl();
    }

};

} } // namespace cppa::detail

#endif // INTERMEDIATE_HPP

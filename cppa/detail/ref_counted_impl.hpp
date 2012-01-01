#ifndef REF_COUNTED_IMPL_HPP
#define REF_COUNTED_IMPL_HPP

namespace cppa { namespace detail {

template<typename T>
class ref_counted_impl
{

    T m_rc;

    ref_counted_impl(ref_counted_impl const&) = delete;

    ref_counted_impl& operator=(ref_counted_impl const&) = delete;

 public:

    virtual ~ref_counted_impl() { }

    inline ref_counted_impl() : m_rc(0) { }

    inline void ref() { ++m_rc; }

    inline bool deref() { return --m_rc > 0; }

    inline bool unique() { return m_rc == 1; }

};

} } // namespace cppa::detail

#endif // REF_COUNTED_IMPL_HPP

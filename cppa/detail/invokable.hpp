#ifndef INVOKABLE_HPP
#define INVOKABLE_HPP

// forward declaration
namespace cppa { class any_tuple; }

namespace cppa { namespace detail {

class intermediate;

// invokable  is NOT thread safe
class invokable// : public ref_counted_impl<size_t>
{

    invokable(const invokable&) = delete;
    invokable& operator=(const invokable&) = delete;

 public:

    invokable() = default;
    virtual ~invokable();
    virtual bool invoke(const any_tuple&) const = 0;
    virtual intermediate* get_intermediate(const any_tuple&) const = 0;

};

template<typename Invoker, typename Getter>
class invokable_impl : public invokable
{

    Invoker m_inv;
    Getter m_get;

 public:

    invokable_impl(Invoker&& i, Getter&& g) : m_inv(i), m_get(g) { }

    virtual bool invoke(const any_tuple& t) const
    {
        return m_inv(t);
    }

    virtual intermediate* get_intermediate(const any_tuple& t) const
    {
        return m_get(t);
    }

};

} } // namespace cppa::detail

#endif // INVOKABLE_HPP

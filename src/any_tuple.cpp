#include "cppa/any_tuple.hpp"

namespace {

struct empty_type_list : cppa::util::abstract_type_list
{

    virtual abstract_type_list* copy() const
    {
        return new empty_type_list;
    }

    virtual const_iterator begin() const
    {
        return 0;
    }

    virtual const cppa::uniform_type_info* at(size_t) const
    {
        throw std::range_error("empty_type_list::at()");
    }

};

struct empty_tuple : cppa::detail::abstract_tuple
{

    empty_type_list m_types;

    virtual size_t size() const
    {
        return 0;
    }

    virtual abstract_tuple* copy() const
    {
        return new empty_tuple;
    }

    virtual void* mutable_at(size_t)
    {
        throw std::range_error("empty_tuple::mutable_at()");
    }

    virtual const void* at(size_t) const
    {
        throw std::range_error("empty_tuple::at()");
    }

    virtual const cppa::uniform_type_info& utype_info_at(size_t) const
    {
        throw std::range_error("empty_tuple::type_at()");
    }

    virtual const cppa::util::abstract_type_list& types() const
    {
        return m_types;
    }

    virtual bool equal_to(const abstract_tuple& other) const
    {
        return other.size() == 0;
    }

};

const cppa::cow_ptr<cppa::detail::abstract_tuple>& s_empty_tuple()
{
    static cppa::cow_ptr<cppa::detail::abstract_tuple> ptr(new empty_tuple);
    return ptr;
}

struct offset_type_list : cppa::util::abstract_type_list
{

    const_iterator begin() const
    {
        return m_begin;
    }

    offset_type_list(const cppa::util::abstract_type_list& decorated)
    {
        auto i = decorated.begin();
        if (i != decorated.end())
        {
            ++i;
        }
        m_begin = i;
    }

 private:

    cppa::util::abstract_type_list::const_iterator m_begin;

};

struct offset_decorator : cppa::detail::abstract_tuple
{

    typedef cppa::cow_ptr<cppa::detail::abstract_tuple> ptr_type;

    offset_decorator(const ptr_type& decorated)
        : m_decorated(decorated), m_type_list(decorated->types())
    {
    }

    void* mutable_at(size_t pos)
    {
        return m_decorated->mutable_at(pos + 1);
    }

    size_t size() const
    {
        return m_decorated->size() - 1;
    }

    abstract_tuple* copy() const
    {
        return new offset_decorator(m_decorated);
    }

    const void* at(size_t pos) const
    {
        return m_decorated->at(pos + 1);
    }

    const cppa::util::abstract_type_list& types() const
    {
        return m_type_list;
    }

    const cppa::uniform_type_info& utype_info_at(size_t pos) const
    {
        return m_decorated->utype_info_at(pos + 1);
    }

 private:

    ptr_type m_decorated;
    offset_type_list m_type_list;

};

} // namespace <anonymous>

namespace cppa {

any_tuple::any_tuple() : m_vals(s_empty_tuple())
{
}

any_tuple::any_tuple(detail::abstract_tuple* ptr) : m_vals(ptr)
{
}

any_tuple::any_tuple(any_tuple&& other) : m_vals(s_empty_tuple())
{
    m_vals.swap(other.m_vals);
}

any_tuple::any_tuple(const cow_ptr<detail::abstract_tuple>& vals) : m_vals(vals)
{
}

any_tuple& any_tuple::operator=(any_tuple&& other)
{
    m_vals.swap(other.m_vals);
    return *this;
}

any_tuple any_tuple::tail() const
{
    if (size() <= 1) return any_tuple(s_empty_tuple());
    return any_tuple(new offset_decorator(m_vals));
}

size_t any_tuple::size() const
{
    return m_vals->size();
}

void* any_tuple::mutable_at(size_t p)
{
    return m_vals->mutable_at(p);
}

const void* any_tuple::at(size_t p) const
{
    return m_vals->at(p);
}

const uniform_type_info& any_tuple::utype_info_at(size_t p) const
{
    return m_vals->utype_info_at(p);
}

const util::abstract_type_list& any_tuple::types() const
{
    return m_vals->types();
}

const cow_ptr<detail::abstract_tuple>& any_tuple::vals() const
{
    return m_vals;
}

bool any_tuple::equal_to(const any_tuple& other) const
{
    return m_vals->equal_to(*other.vals());
}

} // namespace cppa

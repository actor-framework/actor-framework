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

any_tuple& any_tuple::operator=(any_tuple&& other)
{
    m_vals.swap(other.m_vals);
    return *this;
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

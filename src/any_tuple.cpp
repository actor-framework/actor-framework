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

    virtual const cppa::uniform_type_info& type_at(size_t) const
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

    virtual void serialize(cppa::serializer&) const
    {
    }

};

} // namespace <anonymous>

namespace cppa {

any_tuple::any_tuple() : m_vals(new empty_tuple)
{
}

any_tuple::any_tuple(const vals_ptr& vals) : m_vals(vals)
{
}

any_tuple::any_tuple(vals_ptr&& vals) : m_vals(std::move(vals))
{
}

any_tuple::any_tuple(any_tuple&& other) : m_vals(std::move(other.m_vals))
{
}

any_tuple& any_tuple::operator=(any_tuple&& other)
{
    m_vals = std::move(other.m_vals);
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

const uniform_type_info& any_tuple::type_at(size_t p) const
{
    return m_vals->type_at(p);
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

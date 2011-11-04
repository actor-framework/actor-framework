#include "cppa/detail/object_array.hpp"

namespace cppa { namespace detail {

object_array::object_array()
{
}

object_array::object_array(object_array&& other)
    : m_elements(std::move(other.m_elements))
{
}

object_array::object_array(const object_array& other)
    : m_elements(other.m_elements)
{
}

void object_array::push_back(const object& what)
{
    m_elements.push_back(what);
}

void object_array::push_back(object&& what)
{
    m_elements.push_back(std::move(what));
}

void* object_array::mutable_at(size_t pos)
{
    return m_elements[pos].mutable_value();
}

size_t object_array::size() const
{
    return m_elements.size();
}

abstract_tuple* object_array::copy() const
{
    return new object_array(*this);
}

const void* object_array::at(size_t pos) const
{
    return m_elements[pos].value();
}

bool object_array::equal_to(const cppa::detail::abstract_tuple& ut) const
{
    if (size() == ut.size())
    {
        for (size_t i = 0; i < size(); ++i)
        {
            const uniform_type_info& utype = utype_info_at(i);
            if (utype == ut.utype_info_at(i))
            {
                if (!utype.equal(at(i), ut.at(i))) return false;
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

const uniform_type_info& object_array::utype_info_at(size_t pos) const
{
    return m_elements[pos].type();
}

} } // namespace cppa::detail

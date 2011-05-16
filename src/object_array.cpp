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

const util::abstract_type_list& object_array::types() const
{
    return *this;
}

bool object_array::equal_to(const cppa::detail::abstract_tuple& ut) const
{
    if (size() == ut.size())
    {
        for (size_t i = 0; i < size(); ++i)
        {
            const uniform_type_info& utype = type_at(i);
            if (utype == ut.type_at(i))
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

const uniform_type_info& object_array::type_at(size_t pos) const
{
    return m_elements[pos].type();
}

util::abstract_type_list::const_iterator object_array::begin() const
{
    struct type_iterator : util::abstract_type_list::abstract_iterator
    {
        typedef std::vector<object>::const_iterator iterator_type;

        iterator_type m_pos;
        iterator_type m_end;

        type_iterator(iterator_type begin, iterator_type end)
            : m_pos(begin), m_end(end)
        {
        }

        type_iterator(const type_iterator&) = default;

        bool next()
        {
            return ++m_pos != m_end;
        }

        const uniform_type_info& get() const
        {
            return m_pos->type();
        }

        abstract_iterator* copy() const
        {
            return new type_iterator(*this);
        }
    };
    return new type_iterator(m_elements.begin(), m_elements.end());
}

} } // namespace cppa::detail

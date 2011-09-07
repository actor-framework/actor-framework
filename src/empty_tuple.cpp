#include <stdexcept>
#include "cppa/detail/empty_tuple.hpp"

namespace cppa { namespace detail {

util::abstract_type_list* empty_tuple::empty_type_list::copy() const
{
    return new empty_type_list;
}

util::abstract_type_list::const_iterator
empty_tuple::empty_type_list::begin() const
{
    return nullptr;
}

const cppa::uniform_type_info* empty_tuple::empty_type_list::at(size_t) const
{
    throw std::range_error("empty_type_list::at()");
}

size_t empty_tuple::size() const
{
    return 0;
}

abstract_tuple* empty_tuple::copy() const
{
    return new empty_tuple;
}

void* empty_tuple::mutable_at(size_t)
{
    throw std::range_error("empty_tuple::mutable_at()");
}

const void* empty_tuple::at(size_t) const
{
    throw std::range_error("empty_tuple::at()");
}

const uniform_type_info& empty_tuple::utype_info_at(size_t) const
{
    throw std::range_error("empty_tuple::type_at()");
}

const util::abstract_type_list& empty_tuple::types() const
{
    return m_types;
}

bool empty_tuple::equal_to(const abstract_tuple& other) const
{
    return other.size() == 0;
}

} } // namespace cppa::detail

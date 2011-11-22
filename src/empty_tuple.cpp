#include <stdexcept>
#include "cppa/detail/empty_tuple.hpp"

namespace cppa { namespace detail {

size_t empty_tuple::size() const
{
    return 0;
}

void* empty_tuple::mutable_at(size_t)
{
    throw std::range_error("empty_tuple::mutable_at()");
}

abstract_tuple* empty_tuple::copy() const
{
    return new empty_tuple;
}

const void* empty_tuple::at(size_t) const
{
    throw std::range_error("empty_tuple::at()");
}

const uniform_type_info& empty_tuple::utype_info_at(size_t) const
{
    throw std::range_error("empty_tuple::type_at()");
}

bool empty_tuple::equals(const abstract_tuple& other) const
{
    return other.size() == 0;
}

} } // namespace cppa::detail

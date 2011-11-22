#ifndef EMPTY_TUPLE_HPP
#define EMPTY_TUPLE_HPP

#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

struct empty_tuple : cppa::detail::abstract_tuple
{

    size_t size() const;
    void* mutable_at(size_t);
    abstract_tuple* copy() const;
    const void* at(size_t) const;
    bool equals(const abstract_tuple& other) const;
    const uniform_type_info& utype_info_at(size_t) const;

};

} } // namespace cppa::detail

#endif // EMPTY_TUPLE_HPP

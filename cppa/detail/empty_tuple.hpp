#ifndef EMPTY_TUPLE_HPP
#define EMPTY_TUPLE_HPP

#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

struct empty_tuple : cppa::detail::abstract_tuple
{

    size_t size() const;
    void* mutable_at(size_t);
    abstract_tuple* copy() const;
    void const* at(size_t) const;
    bool equals(abstract_tuple const& other) const;
    uniform_type_info const& utype_info_at(size_t) const;

};

} } // namespace cppa::detail

#endif // EMPTY_TUPLE_HPP

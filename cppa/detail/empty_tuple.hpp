#ifndef EMPTY_TUPLE_HPP
#define EMPTY_TUPLE_HPP

#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/util/abstract_type_list.hpp"

namespace cppa { namespace detail {

struct empty_tuple : cppa::detail::abstract_tuple
{

    struct empty_type_list : cppa::util::abstract_type_list
    {
        abstract_type_list* copy() const;
        const_iterator begin() const;
        const cppa::uniform_type_info* at(size_t) const;
    };

    empty_type_list m_types;

    size_t size() const;
    void* mutable_at(size_t);
    abstract_tuple* copy() const;
    const void* at(size_t) const;
    bool equal_to(const abstract_tuple& other) const;
    const util::abstract_type_list& types() const;
    const uniform_type_info& utype_info_at(size_t) const;

};

} } // namespace cppa::detail

#endif // EMPTY_TUPLE_HPP

#ifndef ABSTRACT_TUPLE_HPP
#define ABSTRACT_TUPLE_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

struct abstract_tuple : ref_counted
{

    // mutators
    virtual void* mutable_at(std::size_t pos) = 0;

    // accessors
    virtual std::size_t size() const = 0;
    virtual abstract_tuple* copy() const = 0;
    virtual const void* at(std::size_t pos) const = 0;
    virtual const util::abstract_type_list& types() const = 0;
    virtual bool equal_to(const abstract_tuple& other) const = 0;
    virtual const uniform_type_info* utype_at(std::size_t pos) const = 0;

};

} } // namespace cppa::detail

#endif // ABSTRACT_TUPLE_HPP

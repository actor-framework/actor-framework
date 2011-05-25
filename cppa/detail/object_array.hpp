#ifndef OBJECT_ARRAY_HPP
#define OBJECT_ARRAY_HPP

#include <vector>

#include "cppa/object.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/util/abstract_type_list.hpp"

namespace cppa { namespace detail {

class object_array : public detail::abstract_tuple
                   , public util::abstract_type_list
{

    std::vector<object> m_elements;

 public:

    object_array();

    object_array(object_array&& other);

    object_array(const object_array& other);

    /**
     * @pre
     */
    void push_back(object&& what);

    void push_back(const object& what);

    void* mutable_at(size_t pos);

    size_t size() const;

    abstract_tuple* copy() const;

    const void* at(size_t pos) const;

    const util::abstract_type_list& types() const;

    bool equal_to(const cppa::detail::abstract_tuple&) const;

    const uniform_type_info& utype_info_at(size_t pos) const;

    util::abstract_type_list::const_iterator begin() const;

};

} } // namespace cppa::detail

#endif // OBJECT_ARRAY_HPP

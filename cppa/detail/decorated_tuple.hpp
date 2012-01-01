#ifndef DECORATED_TUPLE_HPP
#define DECORATED_TUPLE_HPP

#include <vector>

#include "cppa/cow_ptr.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/fixed_vector.hpp"

#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/serialize_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
class decorated_tuple : public abstract_tuple
{

 public:

    typedef util::fixed_vector<size_t, sizeof...(ElementTypes)> vector_type;

    typedef util::type_list<ElementTypes...> element_types;

    typedef cow_ptr<abstract_tuple> ptr_type;

    decorated_tuple(ptr_type const& d, vector_type const& v)
        : m_decorated(d), m_mappings(v)
    {
    }

    virtual void* mutable_at(size_t pos)
    {
        return m_decorated->mutable_at(m_mappings[pos]);
    }

    virtual size_t size() const
    {
        return m_mappings.size();
    }

    virtual decorated_tuple* copy() const
    {
        return new decorated_tuple(*this);
    }

    virtual void const* at(size_t pos) const
    {
        return m_decorated->at(m_mappings[pos]);
    }

    virtual uniform_type_info const& utype_info_at(size_t pos) const
    {
        return m_decorated->utype_info_at(m_mappings[pos]);
    }

    virtual bool equals(abstract_tuple const&) const
    {
        return false;
    }

 private:

    ptr_type m_decorated;
    vector_type m_mappings;

    decorated_tuple(decorated_tuple const& other)
        : abstract_tuple()
        , m_decorated(other.m_decorated)
        , m_mappings(other.m_mappings)
    {
    }

    decorated_tuple& operator=(decorated_tuple const&) = delete;

};

} } // namespace cppa::detail

#endif // DECORATED_TUPLE_HPP

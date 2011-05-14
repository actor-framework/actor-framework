#ifndef UNTYPED_TUPLE_HPP
#define UNTYPED_TUPLE_HPP

#include "cppa/cow_ptr.hpp"

#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {

class untyped_tuple
{

    cow_ptr<detail::abstract_tuple> m_vals;

 public:

    untyped_tuple();

    template<typename Tuple>
    untyped_tuple(const Tuple& t) : m_vals(t.vals()) { }

    inline untyped_tuple(const cow_ptr<detail::abstract_tuple>& vals)
        : m_vals(vals)
    {
    }

    std::size_t size() const { return m_vals->size(); }

    void* mutable_at(size_t p) { return m_vals->mutable_at(p); }

    const void* at(size_t p) const { return m_vals->at(p); }

    const uniform_type_info* utype_at(std::size_t p) const { return m_vals->utype_at(p); }

    const util::abstract_type_list& types() const { return m_vals->types(); }

    const cow_ptr<detail::abstract_tuple>& vals() const
    {
        return m_vals;
    }

};

} // namespace cppa

#endif // UNTYPED_TUPLE_HPP

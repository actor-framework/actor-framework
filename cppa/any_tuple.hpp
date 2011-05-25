#ifndef ANY_TUPLE_HPP
#define ANY_TUPLE_HPP

#include "cppa/cow_ptr.hpp"

#include "cppa/tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {

/**
 * @brief Describes a fixed-length tuple (or tuple view)
 *        with elements of any type.
 */
class any_tuple
{

    cow_ptr<detail::abstract_tuple> m_vals;

 public:

    typedef cow_ptr<detail::abstract_tuple> vals_ptr;

    any_tuple();

    template<typename... Args>
    any_tuple(const tuple<Args...>& t) : m_vals(t.vals()) { }

    template<typename... Args>
    any_tuple(const tuple_view<Args...>& t) : m_vals(t.vals()) { }

    any_tuple(vals_ptr&& vals);

    any_tuple(const vals_ptr& vals);

    any_tuple(any_tuple&&);

    any_tuple(const any_tuple&) = default;

    any_tuple& operator=(any_tuple&&);

    any_tuple& operator=(const any_tuple&) = default;

    size_t size() const;

    void* mutable_at(size_t p);

    const void* at(size_t p) const;

    const uniform_type_info& type_info_at(size_t p) const;

    const util::abstract_type_list& types() const;

    const cow_ptr<detail::abstract_tuple>& vals() const;

    bool equal_to(const any_tuple& other) const;

};

inline bool operator==(const any_tuple& lhs, const any_tuple& rhs)
{
    return lhs.equal_to(rhs);
}

inline bool operator!=(const any_tuple& lhs, const any_tuple& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // ANY_TUPLE_HPP

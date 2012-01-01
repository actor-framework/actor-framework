#ifndef ANY_TUPLE_HPP
#define ANY_TUPLE_HPP

#include "cppa/cow_ptr.hpp"

#include "cppa/tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {

template<typename... Types>
tuple_view<Types...> tuple_cast(any_tuple&& tup);

/**
 * @brief Describes a fixed-length tuple with elements of any type.
 */
class any_tuple
{

    template<typename... Types>
    friend tuple_view<Types...> tuple_cast(any_tuple&& tup);

    cow_ptr<detail::abstract_tuple> m_vals;

    explicit any_tuple(cow_ptr<detail::abstract_tuple> const& vals);

 public:

    //typedef cow_ptr<detail::abstract_tuple> vals_ptr;

    any_tuple();

    template<typename... Args>
    any_tuple(tuple<Args...> const& t) : m_vals(t.vals()) { }

    template<typename... Args>
    any_tuple(tuple_view<Args...> const& t) : m_vals(t.vals()) { }

    explicit any_tuple(detail::abstract_tuple*);

    any_tuple(any_tuple&&);

    any_tuple(any_tuple const&) = default;

    any_tuple& operator=(any_tuple&&);

    any_tuple& operator=(any_tuple const&) = default;

    size_t size() const;

    void* mutable_at(size_t p);

    void const* at(size_t p) const;

    uniform_type_info const& utype_info_at(size_t p) const;

    cow_ptr<detail::abstract_tuple> const& vals() const;

    bool equals(any_tuple const& other) const;

    any_tuple tail(size_t offset = 1) const;

    inline bool empty() const
    {
        return size() == 0;
    }

    template<typename T>
    inline T const& get_as(size_t p) const;

    template<typename T>
    inline T& get_mutable_as(size_t p);

};

inline bool operator==(any_tuple const& lhs, any_tuple const& rhs)
{
    return lhs.equals(rhs);
}

inline bool operator!=(any_tuple const& lhs, any_tuple const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline T const& any_tuple::get_as(size_t p) const
{
    return *reinterpret_cast<T const*>(at(p));
}

template<typename T>
inline T& any_tuple::get_mutable_as(size_t p)
{
    return *reinterpret_cast<T*>(mutable_at(p));
}

template<typename... Types>
tuple_view<Types...> tuple_cast(any_tuple const& tup)
{
    return tuple_view<Types...>::from(tup.vals());
}

template<typename... Types>
tuple_view<Types...> tuple_cast(any_tuple&& tup)
{
    return tuple_view<Types...>::from(std::move(tup.m_vals));
}

} // namespace cppa

#endif // ANY_TUPLE_HPP

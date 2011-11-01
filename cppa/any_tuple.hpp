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

    explicit any_tuple(const cow_ptr<detail::abstract_tuple>& vals);

 public:

    //typedef cow_ptr<detail::abstract_tuple> vals_ptr;

    any_tuple();

    template<typename... Args>
    any_tuple(const tuple<Args...>& t) : m_vals(t.vals()) { }

    template<typename... Args>
    any_tuple(const tuple_view<Args...>& t) : m_vals(t.vals()) { }

    explicit any_tuple(detail::abstract_tuple*);

    any_tuple(any_tuple&&);

    any_tuple(const any_tuple&) = default;

    any_tuple& operator=(any_tuple&&);

    any_tuple& operator=(const any_tuple&) = default;

    size_t size() const;

    void* mutable_at(size_t p);

    const void* at(size_t p) const;

    const uniform_type_info& utype_info_at(size_t p) const;

    const util::abstract_type_list& types() const;

    const cow_ptr<detail::abstract_tuple>& vals() const;

    bool equal_to(const any_tuple& other) const;

    any_tuple tail(size_t offset = 1) const;

    inline bool empty() const
    {
        return size() == 0;
    }

    template<typename T>
    inline const T& get_as(size_t p) const;

    template<typename T>
    inline T& get_mutable_as(size_t p);

};

inline bool operator==(const any_tuple& lhs, const any_tuple& rhs)
{
    return lhs.equal_to(rhs);
}

inline bool operator!=(const any_tuple& lhs, const any_tuple& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline const T& any_tuple::get_as(size_t p) const
{
    return *reinterpret_cast<const T*>(at(p));
}

template<typename T>
inline T& any_tuple::get_mutable_as(size_t p)
{
    return *reinterpret_cast<T*>(mutable_at(p));
}

template<typename... Types>
tuple_view<Types...> tuple_cast(const any_tuple& tup)
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

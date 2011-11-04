#ifndef TUPLE_VIEW_HPP
#define TUPLE_VIEW_HPP

#include <vector>

#include "cppa/get.hpp"
#include "cppa/tuple.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/a_matches_b.hpp"
#include "cppa/util/compare_tuples.hpp"

#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t);

/**
 * @brief Describes a view of an fixed-length tuple.
 */
template<typename... ElementTypes>
class tuple_view
{

    template<size_t N, typename... Types>
    friend typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>&);

 public:

    typedef util::type_list<ElementTypes...> element_types;

    // enable use of tuple_view as type_list
    typedef typename element_types::head_type head_type;
    typedef typename element_types::tail_type tail_type;

    static_assert(sizeof...(ElementTypes) > 0,
                  "could not declare an empty tuple_view");

    typedef cow_ptr<detail::abstract_tuple> vals_t;

    tuple_view() : m_vals(tuple<ElementTypes...>().vals()) { }

    static tuple_view from(const vals_t& vals)
    {
        return tuple_view(vals);
    }

    static tuple_view from(vals_t&& vals)
    {
        return tuple_view(std::move(vals));
    }

    tuple_view(const vals_t& vals, std::vector<size_t>&& mappings)
        : m_vals(new detail::decorated_tuple<ElementTypes...>(vals, mappings))
    {
    }

    tuple_view(tuple_view&& other) : m_vals(std::move(other.m_vals))
    {
    }

    tuple_view& operator=(const tuple_view& other)
    {
        m_vals = other.m_vals;
        return *this;
    }

    tuple_view& operator=(tuple_view&& other)
    {
        m_vals = std::move(other.m_vals);
        return *this;
    }

    tuple_view(const tuple_view&) = default;

    inline const vals_t& vals() const
    {
        return m_vals;
    }

    /*inline const element_types& types() const
    {
        //return m_types;
    }*/

    inline size_t size() const
    {
        return m_vals->size();
    }

 private:

    explicit tuple_view(const vals_t& vals) : m_vals(vals)
    {
    }

    explicit tuple_view(vals_t&& vals) : m_vals(std::move(vals))
    {
    }

    vals_t m_vals;

};

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type& get(const tuple_view<Types...>& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::at<N, Types...>::type result_t;
    return *reinterpret_cast<const result_t*>(t.vals()->at(N));
}

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::at<N, Types...>::type result_t;
    return *reinterpret_cast<result_t*>(t.m_vals->mutable_at(N));
}

template<typename TypeList>
struct tuple_view_type_from_type_list;

template<typename... Types>
struct tuple_view_type_from_type_list<util::type_list<Types...>>
{
    typedef tuple_view<Types...> type;
};

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple_view<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple_view<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple_view<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple_view<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // TUPLE_VIEW_HPP

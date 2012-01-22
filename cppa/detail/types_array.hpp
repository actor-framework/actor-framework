#ifndef TYPES_ARRAY_HPP
#define TYPES_ARRAY_HPP

#include <typeinfo>

#include "cppa/type_value_pair.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/is_builtin.hpp"

// forward declarations
namespace cppa {
class uniform_type_info;
uniform_type_info const* uniform_typeid(std::type_info const&);
} // namespace cppa

namespace cppa { namespace detail {

enum type_info_impl { std_tinf, cppa_tinf };

// some metaprogramming utility
template<type_info_impl What, bool IsBuiltin, typename T>
struct ta_util;

template<bool IsBuiltin, typename T>
struct ta_util<std_tinf, IsBuiltin, T>
{
    static inline std::type_info const* get() { return &(typeid(T)); }
};

template<>
struct ta_util<std_tinf, true, anything>
{
    static inline std::type_info const* get() { return nullptr; }
};

template<typename T>
struct ta_util<cppa_tinf, true, T>
{
    static inline uniform_type_info const* get()
    {
        return uniform_typeid(typeid(T));
    }
};

template<>
struct ta_util<cppa_tinf, true, anything>
{
    static inline uniform_type_info const* get() { return nullptr; }
};

template<typename T>
struct ta_util<cppa_tinf, false, T>
{
    static inline uniform_type_info const* get() { return nullptr; }
};

// implements types_array
template<bool BuiltinOnlyTypes, typename... T>
struct types_array_impl
{
    static constexpr bool builtin_only = true;
    // all types are builtin, perform lookup on constuction
    uniform_type_info const* data[sizeof...(T)];
    type_value_pair pairs[sizeof...(T)];
    types_array_impl()
        : data{ta_util<cppa_tinf,util::is_builtin<T>::value,T>::get()...}
    {
        for (size_t i = 0; i < sizeof...(T); ++i)
        {
            pairs[i].first = data[i];
            pairs[i].second = nullptr;
        }
    }
    inline uniform_type_info const* operator[](size_t p) const
    {
        return data[p];
    }
    typedef type_value_pair const* const_iterator;
    inline const_iterator begin() const { return pairs; }
    inline const_iterator end() const { return pairs + sizeof...(T); }
};

template<typename... T>
struct types_array_impl<false, T...>
{
    static constexpr bool builtin_only = false;
    // contains std::type_info for all non-builtin types
    std::type_info const* tinfo_data[sizeof...(T)];
    // contains uniform_type_infos for builtin types and lazy initializes
    // non-builtin types at runtime
    mutable std::atomic<uniform_type_info const*> data[sizeof...(T)];
    mutable std::atomic<type_value_pair const*> pairs;
    // pairs[sizeof...(T)];
    types_array_impl()
        : tinfo_data{ta_util<std_tinf,util::is_builtin<T>::value,T>::get()...}
    {
        bool static_init[sizeof...(T)] = {    !std::is_same<T,anything>::value
                                           && util::is_builtin<T>::value ...  };
        for (size_t i = 0; i < sizeof...(T); ++i)
        {
            if (static_init[i])
            {
                data[i].store(uniform_typeid(*(tinfo_data[i])),
                              std::memory_order_relaxed);
            }
        }
    }
    inline uniform_type_info const* operator[](size_t p) const
    {
        auto result = data[p].load();
        if (result == nullptr)
        {
            auto tinfo = tinfo_data[p];
            if (tinfo != nullptr)
            {
                result = uniform_typeid(*tinfo);
                data[p].store(result, std::memory_order_relaxed);
            }
        }
        return result;
    }
    typedef type_value_pair const* const_iterator;
    inline const_iterator begin() const
    {
        auto result = pairs.load();
        if (result == nullptr)
        {
            auto parr = new type_value_pair[sizeof...(T)];
            for (size_t i = 0; i < sizeof...(T); ++i)
            {
                parr[i].first = (*this)[i];
                parr[i].second = nullptr;
            }
            if (!pairs.compare_exchange_weak(result, parr, std::memory_order_relaxed))
            {
                delete[] parr;
            }
            else
            {
                result = parr;
            }
        }
        return result;
    }
    inline const_iterator end() const
    {
        return begin() + sizeof...(T);
    }
};

// a container for uniform_type_information singletons with optimization
// for builtin types; can act as pattern
template<typename... T>
struct types_array : types_array_impl<util::tl_forall<util::type_list<T...>,
                                                      util::is_builtin>::value,
                                      T...>
{
    static constexpr size_t size = sizeof...(T);
    typedef util::type_list<T...> types;
    typedef typename util::tl_filter_not<types, util::tbind<std::is_same, anything>::type>::type
            filtered_types;
    static constexpr size_t filtered_size = filtered_types::size;
    inline bool has_values() const { return false; }
};

template<typename... T>
struct static_types_array
{
    static types_array<T...> arr;
};

template<typename... T>
types_array<T...> static_types_array<T...>::arr;

template<typename TypeList>
struct static_types_array_from_type_list;

template<typename... T>
struct static_types_array_from_type_list<util::type_list<T...>>
{
    typedef static_types_array<T...> type;
};

} } // namespace cppa::detail

#endif // TYPES_ARRAY_HPP

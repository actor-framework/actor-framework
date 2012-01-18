#ifndef TYPES_ARRAY_HPP
#define TYPES_ARRAY_HPP

#include <typeinfo>

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
    // all types are builtin, perform lookup on constuction
    uniform_type_info const* data[sizeof...(T)];
    types_array_impl()
        : data({ta_util<cppa_tinf,util::is_builtin<T>::value,T>::get()...})
    {
    }
    inline size_t size() const { return sizeof...(T); }
    inline uniform_type_info const* operator[](size_t p) const
    {
        return data[p];
    }
};

template<typename... T>
struct types_array_impl<false, T...>
{
    // contains std::type_info for all non-builtin types
    std::type_info const* tinfo_data[sizeof...(T)];
    // contains uniform_type_infos for builtin types and lazy initializes
    // non-builtin types at runtime
    mutable std::atomic<uniform_type_info const*> data[sizeof...(T)];
    types_array_impl()
        : tinfo_data({ta_util<std_tinf,util::is_builtin<T>::value,T>::get()...})
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
    inline size_t size() const { return sizeof...(T); }
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
};

template<typename... T>
struct types_array : types_array_impl<util::tl_forall<util::type_list<T...>,
                                                      util::is_builtin>::value,
                                      T...>
{
};

template<typename... T>
struct static_types_array
{
    static types_array<T...> arr;
};

template<typename... T>
types_array<T...> static_types_array<T...>::arr;

} } // namespace cppa::detail

#endif // TYPES_ARRAY_HPP

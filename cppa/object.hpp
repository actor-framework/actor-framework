#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <string>
#include <typeinfo>
#include <stdexcept>
#include <type_traits>

#include "cppa/util/disjunction.hpp"

namespace cppa {

// forward declarations
class object;
class uniform_type_info;
namespace detail { template<typename T> struct object_caster; }
bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

/**
 * @brief foobar.
 */
class object
{

    friend class uniform_type_info;

    template<typename T>
    friend struct detail::object_caster;

    void* m_value;
    const uniform_type_info* m_type;

    void swap(object& other);

    object copy() const;

public:

    /**
     * @brief Create an object of type @p utinfo with value @p val.
     * @warning {@link object} takes ownership of @p val.
     * @pre {@code val != nullptr && utinfo != nullptr}
     */
    object(void* val, const uniform_type_info* utinfo);

    /**
     * @brief Create a void object.
     * @post {@code type() == uniform_typeid<util::void_type>()}
     */
    object();

    ~object();

    /**
     * @brief Create an object and move type and value from @p other to this.
     * @post {@code other.type() == uniform_typeid<util::void_type>()}
     */
    object(object&& other);

    /**
     * @brief Create a copy of @p other.
     */
    object(const object& other);

    /**
     * @brief Move the content from @p other to this.
     * @post {@code other.type() == nullptr}
     */
    object& operator=(object&& other);
    object& operator=(const object& other);

    bool equal(const object& other) const;

    const uniform_type_info& type() const;

    std::string to_string() const;

};

inline bool operator==(const object& lhs, const object& rhs)
{
    return lhs.equal(rhs);
}

inline bool operator!=(const object& lhs, const object& rhs)
{
    return !(lhs == rhs);
}

namespace detail {

inline void assert_type(const object& obj, const std::type_info& tinfo)
{
    if (!(obj.type() == tinfo))
    {
        throw std::logic_error("object type doesnt match T");
    }
}

// get a const reference
template<typename T>
struct object_caster<const T&>
{
    static const T& _(const object& obj)
    {
        assert_type(obj, typeid(T));
        return *reinterpret_cast<const T*>(obj.m_value);
    }
};

// get a mutable reference
template<typename T>
struct object_caster<T&>
{
    static T& _(object& obj)
    {
        assert_type(obj, typeid(T));
        return *reinterpret_cast<T*>(obj.m_value);
    }
};

// get a const pointer
template<typename T>
struct object_caster<const T*>
{
    static const T* _(const object& obj)
    {
        assert_type(obj, typeid(T));
        return reinterpret_cast<const T*>(obj.m_value);
    }
};

// get a mutable pointer
template<typename T>
struct object_caster<T*>
{
    static T* _(object& obj)
    {
        assert_type(obj, typeid(T));
        return reinterpret_cast<T*>(obj.m_value);
    }
};

template<typename T>
struct is_const_reference : std::false_type { };

template<typename T>
struct is_const_reference<const T&> : std::true_type { };

template<typename T>
struct is_const_pointer : std::false_type { };

template<typename T>
struct is_const_pointer<const T*> : std::true_type { };

}

template<typename T>
T object_cast(object& obj)
{
    static_assert(util::disjunction<std::is_pointer<T>,
                                    std::is_reference<T>>::value,
                  "T is neither a reference nor a pointer type.");
    return detail::object_caster<T>::_(obj);
}

template<typename T>
const T& object_cast(const object& obj)
{
    static_assert(util::disjunction<detail::is_const_pointer<T>,
                                    detail::is_const_reference<T>>::value,
                  "T is neither a const reference nor a const pointer type.");
    return detail::object_caster<T>::_(obj);
}

} // namespace cppa

#endif // OBJECT_HPP

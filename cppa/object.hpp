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
const uniform_type_info* uniform_typeid(const std::type_info&);
namespace detail { template<typename T> struct object_caster; }
bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

/**
 * @brief A wrapper around a void pointer that stores type informations
 *        and provides copy, move and comparsion operations.
 */
class object
{

    friend class uniform_type_info;

    template<typename T>
    friend struct detail::object_caster;

    void* m_value;
    const uniform_type_info* m_type;

    void swap(object& other);

    /*
     * @brief Copies this object.
     * @returns A (deep) copy of this object.
     */
    object copy() const;

    static void* new_instance(const uniform_type_info* type,
                              const void* from);

public:

    /**
     * @brief Creates an object of type @p utinfo with value @p val.
     * @warning {@link object} takes ownership of @p val.
     * @pre {@code val != nullptr && utinfo != nullptr}
     */
    object(void* val, const uniform_type_info* utinfo);

    /**
     * @brief Creates an empty object.
     * @post {@code empty() && type() == *uniform_typeid<util::void_type>()}
     */
    object();

    /**
     * @brief Creates an object with a copy of @p what.
     * @post {@code empty() == false && type() == *uniform_typeid<T>()}
     */
    template<typename T>
    explicit object(const T& what);

    ~object();

    /**
     * @brief Creates an object and moves type and value
     *        from @p other to @c this.
     * @post {@code other.empty() == true}
     */
    object(object&& other);

    /**
     * @brief Creates a copy of @p other.
     * @post {@code type() == other.type() && equal_to(other)}
     */
    object(const object& other);

    /**
     * @brief Moves the content from @p other to this.
     * @returns @p *this
     * @post {@code other.empty() == true}
     */
    object& operator=(object&& other);

    /**
     *
     */
    object& operator=(const object& other);

    bool equal_to(const object& other) const;

    const uniform_type_info& type() const;

    const void* value() const;

    void* mutable_value();

    bool empty() const;

};

template<typename T>
object::object(const T& what)
{
    m_type = uniform_typeid(typeid(T));
    if (!m_type)
    {
        throw std::logic_error("unknown/unannounced type");
    }
    m_value = new_instance(m_type, &what);
}

inline bool operator==(const object& lhs, const object& rhs)
{
    return lhs.equal_to(rhs);
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

} // namespace detail

template<typename T>
T& get_ref(object& obj)
{
    static_assert(util::disjunction<std::is_pointer<T>,
                                    std::is_reference<T>>::value == false,
                  "T is a reference or a pointer type.");
    detail::assert_type(obj, typeid(T));
    return *reinterpret_cast<T*>(obj.mutable_value());
}

template<typename T>
const T& get(const object& obj)
{
    static_assert(util::disjunction<std::is_pointer<T>,
                                    std::is_reference<T>>::value == false,
                  "T is a reference a pointer type.");
    detail::assert_type(obj, typeid(T));
    return *reinterpret_cast<const T*>(obj.value());
}

} // namespace cppa

#endif // OBJECT_HPP

#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <string>
#include <typeinfo>
#include <stdexcept>
#include <type_traits>

#include "cppa/exception.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/util/disjunction.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

// forward declarations
class object;
bool operator==(object const& lhs, object const& rhs);

class uniform_type_info;
uniform_type_info const* uniform_typeid(std::type_info const&);
bool operator==(uniform_type_info const& lhs, std::type_info const& rhs);

/**
 * @brief Grants mutable access to the stored value of @p obj.
 * @param obj {@link object Object} with <tt>obj.type() == typeid(T)</tt>.
 * @returns A mutable reference to the value stored in @p obj.
 * @relates object
 * @throws std::invalid_argument if <tt>obj.type() != typeid(T)</tt>
 */
template<typename T>
T& get_ref(object& obj);

/**
 * @brief Grants const access to the stored value of @p obj.
 * @param obj {@link object Object} with <tt>obj.type() == typeid(T)</tt>.
 * @returns A const reference to the value stored in @p obj.
 * @relates object
 * @throws std::invalid_argument if <tt>obj.type() != typeid(T)</tt>
 */
template<typename T>
T const& get(object const& obj);

/**
 * @brief An abstraction class that stores an instance of
 *        an announced type.
 */
class object
{

    friend bool operator==(object const& lhs, object const& rhs);

 public:

    ~object();

    /**
     * @brief Creates an object of type @p utinfo with value @p val.
     * @warning {@link object} takes ownership of @p val.
     * @pre {@code val != nullptr && utinfo != nullptr}
     */
    object(void* val, uniform_type_info const* utinfo);

    /**
     * @brief Creates an empty object.
     * @post {type() == *uniform_typeid<util::void_type>()}
     */
    object();

    /**
     * @brief Creates an object and moves type and value
     *        from @p other to @c this.
     */
    object(object&& other);

    /**
     * @brief Creates a (deep) copy of @p other.
     * @post {*this == other}
     */
    object(object const& other);

    /**
     * @brief Moves the content from @p other to this.
     * @returns @p *this
     */
    object& operator=(object&& other);

    /**
     * @brief Creates a (deep) copy of @p other and assigns it to @p this.
     * @return @p *this
     */
    object& operator=(object const& other);

    /**
     * @brief Gets the RTTI of this object.
     * @returns A {@link uniform_type_info} describing the current
     *          type of @p this.
     */
    uniform_type_info const& type() const;

    /**
     * @brief Gets the stored value.
     * @returns A const pointer to the currently stored value.
     * @see get(object const&)
     */
    void const* value() const;

    /**
     * @brief Gets the stored value.
     * @returns A mutable pointer to the currently stored value.
     * @see get_ref(object&)
     */
    void* mutable_value();

    /**
     * @brief Creates an object from @p what.
     * @param what Value of an announced type.
     * @returns An object whose value was initialized with @p what.
     * @post {@code type() == *uniform_typeid<T>()}
     * @throws std::runtime_error if @p T is not announced.
     */
    template<typename T>
    static object from(T&& what);

 private:

    void* m_value;
    uniform_type_info const* m_type;

    void swap(object& other);

};

template<typename T>
object object::from(T&& what)
{
    typedef typename util::rm_ref<T>::type plain_type;
    typedef typename detail::implicit_conversions<plain_type>::type value_type;
    auto rtti = uniform_typeid(typeid(value_type)); // throws on error
    return { new value_type(std::forward<T>(what)), rtti };
}

inline bool operator!=(object const& lhs, object const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
T& get_ref(object& obj)
{
    static_assert(util::disjunction<std::is_pointer<T>,
                                    std::is_reference<T>>::value == false,
                  "T is a reference or a pointer type.");
    if (!(obj.type() == typeid(T)))
    {
        throw std::invalid_argument("obj.type() != typeid(T)");
    }
    return *reinterpret_cast<T*>(obj.mutable_value());
}

template<typename T>
T const& get(object const& obj)
{
    static_assert(util::disjunction<std::is_pointer<T>,
                                    std::is_reference<T>>::value == false,
                  "T is a reference or a pointer type.");
    if (!(obj.type() == typeid(T)))
    {
        throw std::invalid_argument("obj.type() != typeid(T)");
    }
    return *reinterpret_cast<T const*>(obj.value());
}

} // namespace cppa

#endif // OBJECT_HPP

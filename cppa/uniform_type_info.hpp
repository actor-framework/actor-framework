#ifndef UNIFORM_TYPE_INFO_HPP
#define UNIFORM_TYPE_INFO_HPP

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "cppa/object.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/disjunction.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

class serializer;
class deserializer;
class uniform_type_info;

const uniform_type_info* uniform_typeid(const std::type_info&);

/**
 * @brief Provides a platform independent type name and a (very primitive)
 *        kind of reflection in combination with {@link cppa::object object}.
 *
 * The platform independent name is equal to the "in-sourcecode-name"
 * with a few exceptions:
 * - @c std::string is named @c \@str
 * - @c std::u16string is named @c \@u16str
 * - @c std::u32string is named @c \@u32str
 * - @c integers are named <tt>\@(i|u)$size</tt>\n
 *   e.g.: @c \@i32 is a 32 bit signed integer; @c \@u16
 *   is a 16 bit unsigned integer
 * - the <em>anonymous namespace</em> is named @c \@_ \n
 *   e.g.: <tt>namespace { class foo { }; }</tt> is mapped to
 *   @c \@_::foo
 * - {@link cppa::util::void_type} is named @c \@0
 */
class uniform_type_info : cppa::util::comparable<uniform_type_info>
{

    friend class object;

    // needs access to by_type_info()
    friend const uniform_type_info* uniform_typeid(const std::type_info&);

 public:

    class identifier : cppa::util::comparable<identifier>
    {

        friend class uniform_type_info;

        int m_value;

        identifier(int val) : m_value(val) { }

        // disable assignment operators
        identifier& operator=(const identifier&) = delete;

     public:

        // enable copy constructor (only)
        identifier(const identifier&) = default;

        // needed by cppa::detail::comparable<identifier>
        inline int compare(const identifier& other) const
        {
            return m_value - other.m_value;
        }

    };

 private:

    // unique identifier
    identifier m_id;

    // uniform type name
    std::string m_name;

    // disable copy and move constructors
    uniform_type_info(uniform_type_info&&) = delete;
    uniform_type_info(const uniform_type_info&) = delete;

    // disable assignment operators
    uniform_type_info& operator=(uniform_type_info&&) = delete;
    uniform_type_info& operator=(const uniform_type_info&) = delete;

    static const uniform_type_info* by_type_info(const std::type_info& tinfo);

 protected:

    uniform_type_info(const std::string& uniform_name);

 public:

    /**
     * @brief Get instance by uniform name.
     * @param uniform_name The libCPPA internal name for a type.
     * @returnss The instance associated to @p uniform_name.
     */
    static uniform_type_info* by_uniform_name(const std::string& uniform_name);

    /**
     * @brief Get all instances.
     * @returnss A vector with all known (announced) instances.
     */
    static std::vector<uniform_type_info*> instances();

    virtual ~uniform_type_info();

    /**
     * @brief Get the internal libCPPA name for this type.
     * @returnss A string describing the libCPPA internal type name.
     */
    inline const std::string& name() const { return m_name; }

    /**
     * @brief Get the unique identifier of this instance.
     * @returnss The unique identifier of this instance.
     */
    inline const identifier& id() const { return m_id; }

    // needed by cppa::detail::comparable<uniform_type_info>
    inline int compare(const uniform_type_info& other) const
    {
        return id().compare(other.id());
    }

    /**
     * @brief Creates an object of this type.
     */
    object create() const;

    /**
     * @brief Deserializes an object of this type from @p source.
     */
    object deserialize(deserializer* source) const;

    /**
     * @brief Compares two instances of this type.
     */
    virtual bool equal(const void* instance1, const void* instance2) const = 0;

 protected:

    /**
     * @brief Cast @p instance to the native type and delete it.
     */
    virtual void delete_instance(void* instance) const = 0;

    /**
     * @brief Creates an instance of this type, either as a copy of
     *        @p instance or initialized with the default constructor
     *        if @p instance @c == @c nullptr.
     */
    virtual void* new_instance(const void* instance = nullptr) const = 0;

 public:

    /**
     * @brief Determines if this uniform_type_info describes the same
     *        type than @p tinfo.
     */
    virtual bool equal(const std::type_info& tinfo) const = 0;

    /**
     * @brief Serializes @p instance to @p sink.
     */
    virtual void serialize(const void* instance, serializer* sink) const = 0;

    /**
     * @brief Deserializes @p instance from @p source.
     */
    virtual void deserialize(void* instance, deserializer* source) const = 0;

};

template<typename T>
inline const uniform_type_info* uniform_typeid()
{
    return uniform_typeid(typeid(T));
}

bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

inline bool operator!=(const uniform_type_info& lhs, const std::type_info& rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(const std::type_info& lhs, const uniform_type_info& rhs)
{
    return rhs == lhs;
}

inline bool operator!=(const std::type_info& lhs, const uniform_type_info& rhs)
{
    return !(rhs == lhs);
}

} // namespace cppa

#endif // UNIFORM_TYPE_INFO_HPP

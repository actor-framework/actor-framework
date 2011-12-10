#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

// forward declaration
class primitive_variant;

/**
 * @ingroup TypeSystem
 * @brief Technology-independent serialization interface.
 */
class serializer
{

    serializer(const serializer&) = delete;
    serializer& operator=(const serializer&) = delete;

 public:

    serializer() = default;

    virtual ~serializer();

    /**
     * @brief Begins serialization of an object of the type
     *        named @p type_name.
     * @param type_name The platform-independent @p libcppa type name.
     */
    virtual void begin_object(const std::string& type_name) = 0;

    /**
     * @brief Ends serialization of an object.
     */
    virtual void end_object() = 0;

    /**
     * @brief Begins serialization of a sequence of size @p num.
     */
    virtual void begin_sequence(size_t num) = 0;

    /**
     * @brief Ends serialization of a sequence.
     */
    virtual void end_sequence() = 0;

    /**
     * @brief Writes a single value to the data sink.
     * @param value A primitive data value.
     */
    virtual void write_value(const primitive_variant& value) = 0;

    /**
     * @brief Writes @p num values as a tuple to the data sink.
     * @param num Size of the array @p values.
     * @param values An array of size @p num of primitive data values.
     */
    virtual void write_tuple(size_t num, const primitive_variant* values) = 0;

};

template<typename T>
serializer& operator<<(serializer& s, const T& what)
{
    auto mtype = uniform_typeid<T>();
    if (mtype == nullptr)
    {
        throw std::logic_error(  "no uniform type info found for "
                               + cppa::detail::to_uniform_name(typeid(T)));
    }
    mtype->serialize(&what, &s);
    return s;
}

} // namespace cppa

#endif // SERIALIZER_HPP

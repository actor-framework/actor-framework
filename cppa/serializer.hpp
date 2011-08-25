#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

// forward declaration
class primitive_variant;

class serializer
{

    serializer(const serializer&) = delete;
    serializer& operator=(const serializer&) = delete;

 public:

    serializer() = default;

    virtual ~serializer();

    virtual void begin_object(const std::string& type_name) = 0;

    virtual void end_object() = 0;

    virtual void begin_sequence(size_t size) = 0;

    virtual void end_sequence() = 0;

    /**
     * @brief Writes a single value.
     */
    virtual void write_value(const primitive_variant& value) = 0;

    /**
     * @brief Writes @p size values.
     */
    virtual void write_tuple(size_t size, const primitive_variant* values) = 0;

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

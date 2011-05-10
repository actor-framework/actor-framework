#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

namespace cppa {

// forward declaration
class primitive_variant;

class serializer
{

 public:

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

} // namespace cppa

#endif // SERIALIZER_HPP

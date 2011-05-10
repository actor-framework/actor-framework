#ifndef DESERIALIZER_HPP
#define DESERIALIZER_HPP

#include <string>
#include <cstddef>

#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"

namespace cppa {

class deserializer
{

 public:

    virtual ~deserializer();

    /**
     * @brief Seeks the beginning of the next object and return its
     *        uniform type name.
     */
    virtual std::string seek_object() = 0;

    /**
     * @brief Equal to {@link seek_object()} but doesn't
     *        modify the internal in-stream position.
     */
    virtual std::string peek_object() = 0;

    virtual void begin_object(const std::string& type_name) = 0;
    virtual void end_object() = 0;

    virtual size_t begin_sequence() = 0;
    virtual void end_sequence() = 0;

    virtual primitive_variant read_value(primitive_type ptype) = 0;

    virtual void read_tuple(size_t size,
                            const primitive_type* ptypes,
                            primitive_variant* storage   ) = 0;

};

} // namespace cppa

#endif // DESERIALIZER_HPP

#ifndef BINARY_DESERIALIZER_HPP
#define BINARY_DESERIALIZER_HPP

#include "cppa/deserializer.hpp"

namespace cppa {

class binary_deserializer : public deserializer
{

    const char* pos;
    const char* end;

    void range_check(size_t read_size);

 public:

    binary_deserializer(const char* buf, size_t buf_size);
    binary_deserializer(const char* begin, const char* end);

    std::string seek_object();
    std::string peek_object();
    void begin_object(const std::string& type_name);
    void end_object();
    size_t begin_sequence();
    void end_sequence();
    primitive_variant read_value(primitive_type ptype);
    void read_tuple(size_t size,
                    const primitive_type* ptypes,
                    primitive_variant* storage);

};

} // namespace cppa

#endif // BINARY_DESERIALIZER_HPP

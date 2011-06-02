#include <string>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "cppa/util/enable_if.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"

using cppa::util::enable_if;

namespace {

constexpr size_t chunk_size = 512;

} // namespace <anonymous>

namespace cppa {

namespace detail {

class binary_writer
{

    binary_serializer* self;

 public:

    binary_writer(binary_serializer* s) : self(s) { }

    template<typename T>
    inline static void write_int(binary_serializer* self, const T& value)
    {
        memcpy(self->m_wr_pos, &value, sizeof(T));
        self->m_wr_pos += sizeof(T);
    }

    inline static void write_string(binary_serializer* self,
                                    const std::string& str)
    {
        write_int(self, static_cast<std::uint32_t>(str.size()));
        memcpy(self->m_wr_pos, str.c_str(), str.size());
        self->m_wr_pos += str.size();
    }

    template<typename T>
    void operator()(const T& value,
                    typename enable_if< std::is_integral<T> >::type* = 0)
    {
        self->acquire(sizeof(T));
        write_int(self, value);
    }

    template<typename T>
    void operator()(const T&,
                    typename enable_if< std::is_floating_point<T> >::type* = 0)
    {
        throw std::logic_error("binary_serializer::write_floating_point "
                               "not implemented yet");
    }

    void operator()(const std::string& str)
    {
        self->acquire(sizeof(std::uint32_t) + str.size());
        write_string(self, str);
    }

    void operator()(const std::u16string& str)
    {
        self->acquire(sizeof(std::uint32_t) + str.size());
        write_int(self, static_cast<std::uint32_t>(str.size()));
        for (char16_t c : str)
        {
            // force writer to use exactly 16 bit
            write_int(self, static_cast<std::uint16_t>(c));
        }
    }

    void operator()(const std::u32string& str)
    {
        self->acquire(sizeof(std::uint32_t) + str.size());
        write_int(self, static_cast<std::uint32_t>(str.size()));
        for (char32_t c : str)
        {
            // force writer to use exactly 32 bit
            write_int(self, static_cast<std::uint32_t>(c));
        }
    }

};

} // namespace detail

binary_serializer::binary_serializer() : m_begin(0), m_end(0), m_wr_pos(0)
{
}

binary_serializer::~binary_serializer()
{
    delete[] m_begin;
}

void binary_serializer::acquire(size_t num_bytes)
{
    if (!m_begin)
    {
        size_t new_size = chunk_size;
        while (new_size <= num_bytes)
        {
            new_size += chunk_size;
        }
        m_begin = new char[new_size];
        m_end = m_begin + new_size;
        m_wr_pos = m_begin;
    }
    else
    {
        char* next_wr_pos = m_wr_pos + num_bytes;
        if (next_wr_pos > m_end)
        {
            size_t new_size =   static_cast<size_t>(m_end - m_begin)
                              + chunk_size;
            while ((m_begin + new_size) < next_wr_pos)
            {
                new_size += chunk_size;
            }
            char* begin = new char[new_size];
            auto used_bytes = static_cast<size_t>(m_wr_pos - m_begin);
            if (used_bytes > 0)
            {
                memcpy(m_begin, begin, used_bytes);
            }
            delete[] m_begin;
            m_begin = begin;
            m_end = m_begin + new_size;
            m_wr_pos = m_begin + used_bytes;
        }
    }
}

void binary_serializer::begin_object(const std::string& tname)
{
    acquire(sizeof(std::uint32_t) + tname.size());
    detail::binary_writer::write_string(this, tname);
}

void binary_serializer::end_object()
{
}

void binary_serializer::begin_sequence(size_t list_size)
{
    acquire(sizeof(std::uint32_t));
    detail::binary_writer::write_int(this,
                                     static_cast<std::uint32_t>(list_size));
}

void binary_serializer::end_sequence()
{
}

void binary_serializer::write_value(const primitive_variant& value)
{
    value.apply(detail::binary_writer(this));
}

void binary_serializer::write_tuple(size_t size,
                                    const primitive_variant* values)
{
    const primitive_variant* end = values + size;
    for ( ; values != end; ++values)
    {
        write_value(*values);
    }
}

std::pair<size_t, char*> binary_serializer::take_buffer()
{
    char* begin = m_begin;
    char* end = m_end;
    m_begin = m_end = m_wr_pos = nullptr;
    return { static_cast<size_t>(end - begin), begin };
}

size_t binary_serializer::size() const
{
    return static_cast<size_t>(m_wr_pos - m_begin);
}

const char* binary_serializer::data() const
{
    return m_begin;
}

void binary_serializer::reset()
{
    m_wr_pos = m_begin;
}

} // namespace cppa

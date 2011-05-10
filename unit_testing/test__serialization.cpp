#include <new>
#include <set>
#include <list>
#include <locale>
#include <memory>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>
#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"

#include "cppa/util/apply.hpp"
#include "cppa/util/pt_token.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/if_else_type.hpp"
#include "cppa/util/wrapped_type.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_iterable.hpp"

#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/detail/ptype_to_type.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;
using namespace cppa::util;

using cppa::detail::type_to_ptype;
using cppa::detail::ptype_to_type;

struct struct_a
{
    int x;
    int y;
};

bool operator==(const struct_a& lhs, const struct_a& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const struct_a& lhs, const struct_a& rhs)
{
    return !(lhs == rhs);
}

struct struct_b
{
    struct_a a;
    int z;
    std::list<int> ints;
};

bool operator==(const struct_b& lhs, const struct_b& rhs)
{
    return lhs.a == rhs.a && lhs.z == rhs.z && lhs.ints == rhs.ints;
}

bool operator!=(const struct_b& lhs, const struct_b& rhs)
{
    return !(lhs == rhs);
}

struct struct_c
{
    std::map<std::string, std::u16string> strings;
    std::set<int> ints;
};

bool operator==(const struct_c& lhs, const struct_c& rhs)
{
    return lhs.strings == rhs.strings && lhs.ints == rhs.ints;
}

bool operator!=(const struct_c& lhs, const struct_c& rhs)
{
    return !(lhs == rhs);
}

class string_serializer : public serializer
{

    std::ostream& out;

    struct pt_writer
    {

        std::ostream& out;

        pt_writer(std::ostream& mout) : out(mout) { }

        template<typename T>
        void operator()(const T& value)
        {
            out << value;
        }

        void operator()(const std::string& str)
        {
            out << "\"" << str << "\"";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

    int m_open_objects;
    bool m_after_value;

    inline void clear()
    {
        if (m_after_value)
        {
            out << ", ";
            m_after_value = false;
        }
    }

 public:

    string_serializer(std::ostream& mout)
        : out(mout), m_open_objects(0), m_after_value(false) { }

    void begin_object(const std::string& type_name)
    {
        clear();
        ++m_open_objects;
        out << type_name << " ( ";
    }
    void end_object()
    {
        out << " )";
    }

    void begin_sequence(size_t)
    {
        clear();
        out << "{ ";
    }

    void end_sequence()
    {
        out << (m_after_value ? " }" : "}");
    }

    void write_value(const primitive_variant& value)
    {
        clear();
        value.apply(pt_writer(out));
        m_after_value = true;
    }

    void write_tuple(size_t size, const primitive_variant* values)
    {
        clear();
        out << " {";
        const primitive_variant* end = values + size;
        for ( ; values != end; ++values)
        {
            write_value(*values);
        }
        out << (m_after_value ? " }" : "}");
    }

};

/*
class xml_serializer : public serializer
{

    std::ostream& out;
    std::string indentation;

    inline void inc_indentation()
    {
        indentation.resize(indentation.size() + 4, ' ');
    }

    inline void dec_indentation()
    {
        auto isize = indentation.size();
        indentation.resize((isize > 4) ? (isize - 4) : 0);
    }

    struct pt_writer
    {

        std::ostream& out;
        const std::string& indentation;

        pt_writer(std::ostream& ostr, const std::string& indent)
            : out(ostr), indentation(indent)
        {
        }

        template<typename T>
        void operator()(const T& value)
        {
            out << indentation << "<value type=\""
                << primitive_type_name(type_to_ptype<T>::ptype)
                << "\">" << value << "</value>\n";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

 public:

    xml_serializer(std::ostream& ostr) : out(ostr), indentation("") { }

    void begin_object(const std::string& type_name)
    {
        out << indentation << "<object type=\"" << type_name << "\">\n";
        inc_indentation();
    }
    void end_object()
    {
        dec_indentation();
        out << indentation << "</object>\n";
    }

    void begin_list(size_t)
    {
        out << indentation << "<list>\n";
        inc_indentation();
    }

    void end_list()
    {
        dec_indentation();
        out << indentation << "</list>\n";
    }

    void write_value(const pt_value& value)
    {
        value.apply(pt_writer(out, indentation));
    }

};
*/

class binary_deserializer : public deserializer
{

    const char* m_buf;
    size_t m_rd_pos;
    size_t m_buf_size;

    void range_check(size_t read_size)
    {
        if (m_rd_pos + read_size >= m_buf_size)
        {
            std::out_of_range("binary_deserializer::read()");
        }
    }

    template<typename T>
    void read(T& storage, bool modify_rd_pos = true)
    {
        range_check(sizeof(T));
        memcpy(&storage, m_buf + m_rd_pos, sizeof(T));
        if (modify_rd_pos) m_rd_pos += sizeof(T);
    }

    void read(std::string& str, bool modify_rd_pos = true)
    {
        std::uint32_t str_size;
        read(str_size, modify_rd_pos);
        const char* cpy_begin;
        if (modify_rd_pos)
        {
            range_check(str_size);
            cpy_begin = m_buf + m_rd_pos;
        }
        else
        {
            range_check(sizeof(std::uint32_t) + str_size);
            cpy_begin = m_buf + m_rd_pos + sizeof(std::uint32_t);
        }
        str.clear();
        str.reserve(str_size);
        const char* cpy_end = cpy_begin + str_size;
        std::copy(cpy_begin, cpy_end, std::back_inserter(str));
        if (modify_rd_pos) m_rd_pos += str_size;
    }

    template<typename CharType, typename StringType>
    void read_unicode_string(StringType& str)
    {
        std::uint32_t str_size;
        read(str_size);
        str.reserve(str_size);
        for (size_t i = 0; i < str_size; ++i)
        {
            CharType c;
            read(c);
            str += static_cast<typename StringType::value_type>(c);
        }
    }

    void read(std::u16string& str)
    {
        // char16_t is guaranteed to has *at least* 16 bytes,
        // but not to have *exactly* 16 bytes; thus use uint16_t
        read_unicode_string<std::uint16_t>(str);
    }

    void read(std::u32string& str)
    {
        // char32_t is guaranteed to has *at least* 32 bytes,
        // but not to have *exactly* 32 bytes; thus use uint32_t
        read_unicode_string<std::uint32_t>(str);
    }

    struct pt_reader
    {
        binary_deserializer* self;
        inline pt_reader(binary_deserializer* mself) : self(mself) { }
        template<typename T>
        inline void operator()(T& value)
        {
            self->read(value);
        }
    };

 public:

    binary_deserializer(const char* buf, size_t buf_size)
        : m_buf(buf), m_rd_pos(0), m_buf_size(buf_size)
    {
    }

    std::string seek_object()
    {
        std::string result;
        read(result);
        return result;
    }

    std::string peek_object()
    {
        std::string result;
        read(result, false);
        return result;
    }

    void begin_object(const std::string&)
    {
    }

    void end_object()
    {
    }

    size_t begin_sequence()
    {
        std::uint32_t size;
        read(size);
        return size;
    }

    void end_sequence()
    {
    }

    primitive_variant read_value(primitive_type ptype)
    {
        primitive_variant val(ptype);
        val.apply(pt_reader(this));
        return val;
    }

    void read_tuple(size_t size,
                    const primitive_type* ptypes,
                    primitive_variant* storage)
    {
        const primitive_type* end = ptypes + size;
        for ( ; ptypes != end; ++ptypes)
        {
            *storage = std::move(read_value(*ptypes));
            ++storage;
        }
    }

};

class string_deserializer : public deserializer
{

    std::string m_str;
    std::string::iterator m_pos;
    size_t m_obj_count;

    void skip_space_and_comma()
    {
        while (*m_pos == ' ' || *m_pos == ',') ++m_pos;
    }

    void throw_malformed(const std::string& error_msg)
    {
        throw std::logic_error("malformed string: " + error_msg);
    }

    void consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos != c)
        {
            std::string error;
            error += "expected '";
            error += c;
            error += "' found '";
            error += *m_pos;
            error += "'";
            throw_malformed(error);
        }
        ++m_pos;
    }

    inline std::string::iterator next_delimiter()
    {
        return std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case '(':
             case ')':
             case '{':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
    }

 public:

    string_deserializer(const std::string& str) : m_str(str)
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    string_deserializer(std::string&& str) : m_str(std::move(str))
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    std::string seek_object()
    {
        skip_space_and_comma();
        auto substr_end = next_delimiter();
        // next delimiter must eiter be '(' or "\w+\("
        if (substr_end == m_str.end() ||  *substr_end != '(')
        {
            auto peeker = substr_end;
            while (peeker != m_str.end() && *peeker == ' ') ++peeker;
            if (peeker == m_str.end() ||  *peeker != '(')
            {
                throw_malformed("type name not followed by '('");
            }
        }
        std::string result(m_pos, substr_end);
        m_pos = substr_end;
        return result;
    }

    std::string peek_object()
    {
        std::string result = seek_object();
        // restore position in stream
        m_pos -= result.size();
        return result;
    }

    void begin_object(const std::string&)
    {
        ++m_obj_count;
        skip_space_and_comma();
        consume('(');
    }

    void end_object()
    {
        consume(')');
        if (--m_obj_count == 0)
        {
            skip_space_and_comma();
            if (m_pos != m_str.end())
            {
                throw_malformed("expected end of of string");
            }
        }
    }

    size_t begin_sequence()
    {
        consume('{');
        auto list_end = std::find(m_pos, m_str.end(), '}');
        return std::count(m_pos, list_end, ',') + 1;
    }

    void end_sequence()
    {
        consume('}');
    }

    struct from_string
    {
        const std::string& str;
        from_string(const std::string& s) : str(s) { }
        template<typename T>
        void operator()(T& what)
        {
            std::istringstream s(str);
            s >> what;
        }
        void operator()(std::string& what)
        {
            what = str;
        }
        void operator()(std::u16string&) { }
        void operator()(std::u32string&) { }
    };

    primitive_variant read_value(primitive_type ptype)
    {
        skip_space_and_comma();
        auto substr_end = std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
        std::string substr(m_pos, substr_end);
        primitive_variant result(ptype);
        result.apply(from_string(substr));
        m_pos += substr.size();
        return result;
    }

    void foo() {}

    void read_tuple(size_t size, const primitive_type* begin, primitive_variant* storage)
    {
        consume('{');
        const primitive_type* end = begin + size;
        for ( ; begin != end; ++begin)
        {
            *storage = std::move(read_value(*begin));
            ++storage;
        }
        consume('}');
    }

};

std::map<std::string, std::unique_ptr<uniform_type_info> > s_types;

void announce(uniform_type_info* utype)
{
    const auto& uname = utype->name();
    if (s_types.count(uname) == 0)
    {
        std::unique_ptr<uniform_type_info> uptr(utype);
        s_types.insert(std::make_pair(uname, std::move(uptr)));
    }
    else
    {
        cerr << utype->name() << " already announced" << endl;
        delete utype;
    }
}

uniform_type_info* get_meta_type(const std::string& tname)
{
    auto i = s_types.find(tname);
    return (i != s_types.end()) ? i->second.get() : nullptr;
}

uniform_type_info* get_meta_type(const std::type_info& tinfo)
{
    return get_meta_type(detail::to_uniform_name(tinfo));
}

template<typename T>
uniform_type_info* get_meta_type()
{
    return get_meta_type(detail::to_uniform_name(typeid(T)));
}

template<int>
inline void _announce_all() { }

template<int, typename T0, typename... Tn>
inline void _announce_all()
{
    announce(new detail::default_uniform_type_info_impl<T0>);
    _announce_all<0, Tn...>();
}

template<typename... Tn>
void announce_all()
{
    _announce_all<0, Tn...>();
}

namespace {

class root_object_type
{

 public:

    root_object_type()
    {
        announce_all<std::int8_t, std::int16_t, std::int32_t, std::int64_t,
                     std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
                     float, double, long double,
                     std::string, std::u16string, std::u32string>();
    }

    template<typename T>
    void serialize(const T& what, serializer* where) const
    {
        uniform_type_info* mtype = get_meta_type(detail::to_uniform_name(typeid(T)));
        if (!mtype)
        {
            throw std::logic_error("no meta found for "
                                   + cppa::detail::to_uniform_name(typeid(T)));
        }
        mtype->serialize(&what, where);
    }

    /**
     * @brief Deserializes a new object from @p source and returns the
     *        new (deserialized) instance with its meta_type.
     */
    object deserialize(deserializer* source) const
    {
        std::string tname = source->peek_object();
        uniform_type_info* mtype = get_meta_type(tname);
        if (!mtype)
        {
            throw std::logic_error("no meta object found for " + tname);
        }
        return mtype->deserialize(source);
    }

}
root_object;

} // namespace <anonymous>

template<typename T>
std::string to_string(const T& what)
{
    std::string tname = detail::to_uniform_name(typeid(T));
    auto mobj = get_meta_type(tname);
    if (!mobj) throw std::logic_error(tname + " not found");
    std::ostringstream osstr;
    string_serializer strs(osstr);
    mobj->serialize(&what, &strs);
    return osstr.str();
}

template<typename T, typename... Args>
uniform_type_info* meta_object(const Args&... args)
{
    return new detail::default_uniform_type_info_impl<T>(args...);
}

template<class C, class Parent, typename... Args>
std::pair<C Parent::*, uniform_type_info_base<C>*>
compound_member(C Parent::*c_ptr, const Args&... args)
{
    return std::make_pair(c_ptr, meta_object<C>(args...));
}

std::size_t test__serialization()
{
    CPPA_TEST(test__serialization);

    CPPA_CHECK_EQUAL((is_iterable<int>::value), false);
    // std::string is primitive and thus not identified by is_iterable
    CPPA_CHECK_EQUAL((is_iterable<std::string>::value), false);
    CPPA_CHECK_EQUAL((is_iterable<std::list<int>>::value), true);
    CPPA_CHECK_EQUAL((is_iterable<std::map<int,int>>::value), true);
    // test meta_object implementation for primitive types
    {
        auto meta_int = get_meta_type<std::uint32_t>();
        CPPA_CHECK(meta_int != nullptr);
        if (meta_int)
        {
            /*
            auto o = meta_int->create();
            auto str = to_string(*i);
            CPPA_CHECK_EQUAL(*i, 0);
            CPPA_CHECK_EQUAL(str, "@u32 ( 0 )");
            meta_int->delete_instance(i);
            */
        }
    }
    // test serializers / deserializers with struct_b
    {
        // get meta object for struct_b
        announce(meta_object<struct_b>(compound_member(&struct_b::a,
                                                       &struct_a::x,
                                                       &struct_a::y),
                                       &struct_b::z,
                                       &struct_b::ints));
        // testees
        struct_b b1 = { { 1, 2 }, 3, { 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;
        // expected result of to_string(&b1, meta_b)
        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";
        // verify
        CPPA_CHECK_EQUAL((to_string(b1)), b1str);
        // binary buffer
        std::pair<size_t, char*> buf;
        {
            // serialize b1 to buf
            binary_serializer bs;
            root_object.serialize(b1, &bs);
            // deserialize b2 from buf
            binary_deserializer bd(bs.data(), bs.size());
            object res = root_object.deserialize(&bd);
            CPPA_CHECK_EQUAL(res.type().name(), "struct_b");
            b2 = object_cast<const struct_b&>(res);
        }
        // cleanup
        delete buf.second;
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(b1, b2);
        CPPA_CHECK_EQUAL(to_string(b2), b1str);
        // deserialize b3 from string, using string_deserializer
        {
            string_deserializer strd(b1str);
            auto res = root_object.deserialize(&strd);
            CPPA_CHECK_EQUAL(res.type().name(), "struct_b");
            b3 = object_cast<const struct_b&>(res);
        }
        CPPA_CHECK_EQUAL(b1, b3);
    }
    // test serializers / deserializers with struct_c
    {
        // get meta type of struct_c and "announce"
        announce(meta_object<struct_c>(&struct_c::strings,&struct_c::ints));
        // testees
        struct_c c1 = { { { "abc", u"cba" }, { "x", u"y" } }, { 9, 4, 5 } };
        struct_c c2;
        // binary buffer
        std::pair<size_t, char*> buf;
        // serialize c1 to buf
        {
            binary_serializer bs;
            root_object.serialize(c1, &bs);
            buf = bs.take_buffer();
        }
        // serialize c2 from buf
        {
            binary_deserializer bd(buf.second, buf.first);
            auto res = root_object.deserialize(&bd);
            CPPA_CHECK_EQUAL(res.type().name(), "struct_c");
            c2 = object_cast<const struct_c&>(res);
        }
        delete buf.second;
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(c1, c2);
    }
    return CPPA_TEST_RESULT;
}

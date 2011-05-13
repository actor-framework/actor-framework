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
#include "cppa/announce.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

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

template<typename T>
std::string to_string(const T& what)
{
    std::string tname = detail::to_uniform_name(typeid(T));
    auto mobj = uniform_typeid<T>();
    if (!mobj) throw std::logic_error(tname + " not found");
    std::ostringstream osstr;
    string_serializer strs(osstr);
    mobj->serialize(&what, &strs);
    return osstr.str();
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
        auto meta_int = uniform_typeid<std::uint32_t>();
        CPPA_CHECK(meta_int != nullptr);
        if (meta_int)
        {
            auto o = meta_int->create();
            get_ref<std::uint32_t>(o) = 42;
            auto str = to_string(get<std::uint32_t>(o));
            CPPA_CHECK_EQUAL(str, "@u32 ( 42 )");
        }
    }
    // test serializers / deserializers with struct_b
    {
        // get meta object for struct_b
        announce<struct_b>(compound_member(&struct_b::a,
                                           &struct_a::x,
                                           &struct_a::y),
                           &struct_b::z,
                           &struct_b::ints);
        // testees
        struct_b b1 = { { 1, 2 }, 3, { 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;
        // expected result of to_string(&b1, meta_b)
        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";
        // verify
        CPPA_CHECK_EQUAL((to_string(b1)), b1str);
        {
            // serialize b1 to buf
            binary_serializer bs;
            bs << b1;
            // deserialize b2 from buf
            binary_deserializer bd(bs.data(), bs.size());
            object res;
            bd >> res;
            CPPA_CHECK_EQUAL(res.type().name(), "struct_b");
            b2 = get<struct_b>(res);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(b1, b2);
        CPPA_CHECK_EQUAL(to_string(b2), b1str);
        // deserialize b3 from string, using string_deserializer
        {
            string_deserializer strd(b1str);
            object res;
            strd >> res;
            CPPA_CHECK_EQUAL(res.type().name(), "struct_b");
            b3 = get<struct_b>(res);
        }
        CPPA_CHECK_EQUAL(b1, b3);
    }
    // test serializers / deserializers with struct_c
    {
        // get meta type of struct_c and "announce"
        announce<struct_c>(&struct_c::strings, &struct_c::ints);
        // testees
        struct_c c1 = { { { "abc", u"cba" }, { "x", u"y" } }, { 9, 4, 5 } };
        struct_c c2;
        {
            // serialize c1 to buf
            binary_serializer bs;
            bs << c1;
            // serialize c2 from buf
            binary_deserializer bd(bs.data(), bs.size());
            object res;
            bd >> res;
            CPPA_CHECK_EQUAL(res.type().name(), "struct_c");
            c2 = get<struct_c>(res);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(c1, c2);
    }
    return CPPA_TEST_RESULT;
}

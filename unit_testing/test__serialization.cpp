#include <new>
#include <set>
#include <list>
#include <stack>
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
#include "cppa/message.hpp"
#include "cppa/announce.hpp"
#include "cppa/get_view.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/apply.hpp"
#include "cppa/util/pt_token.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/is_iterable.hpp"
#include "cppa/util/if_else_type.hpp"
#include "cppa/util/wrapped_type.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/detail/object_array.hpp"
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
            out << "\"";// << str << "\"";
            for (char c : str)
            {
                if (c == '"') out << "\\\"";
                else out << c;
            }
            out << '"';
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

    int m_open_objects;
    bool m_after_value;
    bool m_obj_just_opened;

    inline void clear()
    {
        if (m_after_value)
        {
            out << ", ";
            m_after_value = false;
        }
        else if (m_obj_just_opened)
        {
            out << " ( ";
            m_obj_just_opened = false;
        }
    }

 public:

    string_serializer(std::ostream& mout)
        : out(mout), m_open_objects(0)
        , m_after_value(false), m_obj_just_opened(false)
    {
    }

    void begin_object(const std::string& type_name)
    {
        clear();
        ++m_open_objects;
        out << type_name;// << " ( ";
        m_obj_just_opened = true;
    }
    void end_object()
    {
        if (m_obj_just_opened)
        {
            m_obj_just_opened = false;
        }
        else
        {
            out << (m_after_value ? " )" : ")");
        }
        m_after_value = true;
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
    std::stack<bool> m_obj_had_left_parenthesis;

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

    bool try_consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos == c)
        {
            ++m_pos;
            return true;
        }
        return false;
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

    void integrity_check()
    {
        if (m_obj_had_left_parenthesis.empty())
        {
            throw_malformed("missing begin_object()");
        }
        else if (m_obj_had_left_parenthesis.top() == false)
        {
            throw_malformed("expected left parenthesis after "
                            "begin_object call or void value");
        }
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
        if (m_pos == substr_end)
        {
            if (m_pos != m_str.end())
            {
                std::string remain(m_pos, m_str.end());
                cout << remain << endl;
            }
            throw_malformed("wtf?");
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
        m_obj_had_left_parenthesis.push(try_consume('('));
        //consume('(');
    }

    void end_object()
    {
        if (m_obj_had_left_parenthesis.empty())
        {
            throw_malformed("missing begin_object()");
        }
        else
        {
            if (m_obj_had_left_parenthesis.top() == true)
            {
                consume(')');
            }
            m_obj_had_left_parenthesis.pop();
        }
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
        integrity_check();
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
        integrity_check();
        skip_space_and_comma();
        std::string::iterator substr_end;
        auto find_if_cond = [] (char c) -> bool
        {
            switch (c)
            {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        };
        if (ptype == pt_u8string)
        {
            if (*m_pos == '"')
            {
                // skip leading "
                ++m_pos;
                char last_char = '"';
                auto find_if_str_cond = [&last_char] (char c) -> bool
                {
                    if (c == '"' && last_char != '\\')
                    {
                        return true;
                    }
                    last_char = c;
                    return false;
                };
                substr_end = std::find_if(m_pos, m_str.end(), find_if_str_cond);
            }
            else
            {
                substr_end = std::find_if(m_pos, m_str.end(), find_if_cond);
            }
        }
        else
        {
            substr_end = std::find_if(m_pos, m_str.end(), find_if_cond);
        }
        if (substr_end == m_str.end())
        {
            throw std::logic_error("malformed string (unterminated value)");
        }
        std::string substr(m_pos, substr_end);
        m_pos += substr.size();
        if (ptype == pt_u8string)
        {
            // skip trailing "
            if (*m_pos != '"')
            {
                std::string error_msg;
                error_msg  = "malformed string, expected '\"' found '";
                error_msg += *m_pos;
                error_msg += "'";
                throw std::logic_error(error_msg);
            }
            ++m_pos;
            // replace '\"' by '"'
            char last_char = ' ';
            auto cond = [&last_char] (char c) -> bool
            {
                if (c == '"' && last_char == '\\')
                {
                    return true;
                }
                last_char = c;
                return false;
            };
            std::string tmp;
            auto sbegin = substr.begin();
            auto send = substr.end();
            for (auto i = std::find_if(sbegin, send, cond);
                 i != send;
                 i = std::find_if(i, send, cond))
            {
                --i;
                tmp.append(sbegin, i);
                tmp += '"';
                i += 2;
                sbegin = i;
            }
            if (sbegin != substr.begin())
            {
                tmp.append(sbegin, send);
            }
            if (!tmp.empty())
            {
                substr = std::move(tmp);
            }
        }
        primitive_variant result(ptype);
        result.apply(from_string(substr));
        return result;
    }

    void foo() {}

    void read_tuple(size_t size, const primitive_type* begin, primitive_variant* storage)
    {
        integrity_check();
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

class message_tinfo : public util::abstract_uniform_type_info<message>
{

 public:

    virtual void serialize(const void* instance, serializer* sink) const
    {
        const message& msg = *reinterpret_cast<const message*>(instance);
        const any_tuple& data = msg.content();
        sink->begin_object(name());
        uniform_typeid<actor_ptr>()->serialize(&(msg.sender()), sink);
        uniform_typeid<channel_ptr>()->serialize(&(msg.receiver()), sink);
        uniform_typeid<any_tuple>()->serialize(&data, sink);
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const
    {
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        actor_ptr sender;
        channel_ptr receiver;
        any_tuple content;
        uniform_typeid<actor_ptr>()->deserialize(&sender, source);
        uniform_typeid<channel_ptr>()->deserialize(&receiver, source);
        uniform_typeid<any_tuple>()->deserialize(&content, source);
        source->end_object();
        *reinterpret_cast<message*>(instance) = message(sender,
                                                        receiver,
                                                        content);
    }

};

template<typename T>
std::string to_string(const T& what)
{
    auto utype = uniform_typeid<T>();
    if (!utype)
    {
        throw std::logic_error(  detail::to_uniform_name(typeid(T))
                               + " not found");
    }
    std::ostringstream osstr;
    string_serializer strs(osstr);
    utype->serialize(&what, &strs);
    return osstr.str();
}

size_t test__serialization()
{
    CPPA_TEST(test__serialization);
    announce(typeid(message), new message_tinfo);


    auto oarr = new detail::object_array;
    oarr->push_back(object(static_cast<std::uint32_t>(42)));
    oarr->push_back(object(std::string("foo")));

    any_tuple atuple1(oarr);
    try
    {
        auto tv1 = get_view<std::uint32_t, std::string>(atuple1);
        CPPA_CHECK_EQUAL(tv1.size(), 2);
        CPPA_CHECK_EQUAL(get<0>(tv1), 42);
        CPPA_CHECK_EQUAL(get<1>(tv1), "foo");
    }
    catch (std::exception& e)
    {
        CPPA_ERROR("exception: " << e.what());
    }

    {
        // serialize b1 to buf
        binary_serializer bs;
        bs << atuple1;
        // deserialize b2 from buf
        binary_deserializer bd(bs.data(), bs.size());
        any_tuple atuple2;
        uniform_typeid<any_tuple>()->deserialize(&atuple2, &bd);
        try
        {
            auto tview = get_view<std::uint32_t, std::string>(atuple2);
            CPPA_CHECK_EQUAL(tview.size(), 2);
            CPPA_CHECK_EQUAL(get<0>(tview), 42);
            CPPA_CHECK_EQUAL(get<1>(tview), "foo");
        }
        catch (std::exception& e)
        {
            CPPA_ERROR("exception: " << e.what());
        }
    }

    {
        message msg1(0, 0, 42, std::string("Hello \"World\"!"));
        cout << "msg = " << to_string(msg1) << endl;
        binary_serializer bs;
        bs << msg1;
        binary_deserializer bd(bs.data(), bs.size());
        string_deserializer sd(to_string(msg1));
        object obj1;
        bd >> obj1;
        object obj2;
        sd >> obj2;
        CPPA_CHECK_EQUAL(obj1, obj2);
        if (obj1.type() == typeid(message) && obj2.type() == obj1.type())
        {
            auto& content1 = get<message>(obj1).content();
            auto& content2 = get<message>(obj2).content();
            auto cview1 = get_view<decltype(42), std::string>(content1);
            auto cview2 = get_view<decltype(42), std::string>(content2);
            CPPA_CHECK_EQUAL(cview1.size(), 2);
            CPPA_CHECK_EQUAL(cview2.size(), 2);
            CPPA_CHECK_EQUAL(get<0>(cview1), 42);
            CPPA_CHECK_EQUAL(get<0>(cview2), 42);
            CPPA_CHECK_EQUAL(get<1>(cview1), "Hello \"World\"!");
            CPPA_CHECK_EQUAL(get<1>(cview2), "Hello \"World\"!");
        }
        else
        {
            CPPA_ERROR("obj.type() != typeid(message)");
        }
    }

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

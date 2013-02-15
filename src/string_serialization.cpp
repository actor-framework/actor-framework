/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <stack>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/object.hpp"
#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/from_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/network/default_actor_addressing.hpp"

using namespace std;

namespace cppa {

namespace {

bool isbuiltin(const string& type_name) {
    return type_name == "@str" || type_name == "@atom" || type_name == "@<>";
}

// serializes types as type_name(...) except:
// - strings are serialized "..."
// - atoms are serialized '...'
class string_serializer : public serializer {

    typedef serializer super;

    ostream& out;
    network::default_actor_addressing m_addressing;

    struct pt_writer {

        ostream& out;
        bool suppress_quotes;

        pt_writer(ostream& mout, bool suppress_quotes = false) : out(mout), suppress_quotes(suppress_quotes) { }

        template<typename T>
        void operator()(const T& value) {
            out << value;
        }

        void operator()(const string& str) {
            if (!suppress_quotes) out << "\"";
            for (char c : str) {
                if (c == '"') out << "\\\"";
                else out << c;
            }
            if (!suppress_quotes) out << "\"";
        }

        void operator()(const u16string&) { }

        void operator()(const u32string&) { }

    };

    bool m_after_value;
    bool m_obj_just_opened;
    stack<string> m_open_objects;

    inline void clear() {
        if (m_after_value) {
            out << ", ";
            m_after_value = false;
        }
        else if (m_obj_just_opened) {
            if (!m_open_objects.empty() && !isbuiltin(m_open_objects.top())) {
                out << " ( ";
            }
            m_obj_just_opened = false;
        }
    }

 public:

    string_serializer(ostream& mout)
    : super(&m_addressing), out(mout), m_after_value(false), m_obj_just_opened(false) { }

    void begin_object(const string& type_name) {
        clear();
        m_open_objects.push(type_name);
        // do not print type names for strings and atoms
        if (!isbuiltin(type_name)) out << type_name;
        m_obj_just_opened = true;
    }

    void end_object() {
        if (m_obj_just_opened) {
            // no open brackets to close
            m_obj_just_opened = false;
        }
        m_after_value = true;
        if (!m_open_objects.empty()) {
            if (!isbuiltin(m_open_objects.top())) {
                out << (m_after_value ? " )" : ")");
            }
            m_open_objects.pop();
        }
    }

    void begin_sequence(size_t) {
        clear();
        out << "{ ";
    }

    void end_sequence() {
        out << (m_after_value ? " }" : "}");
        m_after_value = true;
    }

    void write_value(const primitive_variant& value) {
        clear();
        if (m_open_objects.empty()) {
            throw runtime_error("write_value(): m_open_objects.empty()");
        }
        if (m_open_objects.top() == "@atom") {
            if (value.ptype() != pt_uint64) {
                throw runtime_error("expected uint64 value after @atom");
            }
            // write atoms as strings instead of integer values
            auto av = static_cast<atom_value>(get<uint64_t>(value));
            out << "'";
            (pt_writer(out, true))(to_string(av));
            out << "'";
        }
        else {
            value.apply(pt_writer(out));
        }
        m_after_value = true;
    }

    void write_tuple(size_t size, const primitive_variant* values) {
        clear();
        out << "{";
        const primitive_variant* end = values + size;
        for ( ; values != end; ++values) {
            write_value(*values);
        }
        out << (m_after_value ? " }" : "}");
    }

    void write_raw(size_t num_bytes, const void* buf) {
        clear();
        auto first = reinterpret_cast<const unsigned char*>(buf);
        auto last = first + num_bytes;
        out << hex;
        out << setfill('0');
        for (; first != last; ++first) {
            out << setw(2) << static_cast<size_t>(*first);
        }
        out << dec;
        m_after_value = true;
    }

};

class string_deserializer : public deserializer {

    typedef deserializer super;

    string m_str;
    string::iterator m_pos;
    //size_t m_obj_count;
    stack<bool> m_obj_had_left_parenthesis;
    stack<string> m_open_objects;
    network::default_actor_addressing m_addressing;

    void skip_space_and_comma() {
        while (*m_pos == ' ' || *m_pos == ',') ++m_pos;
    }

    void throw_malformed(const string& error_msg) {
        throw logic_error("malformed string: " + error_msg);
    }

    void consume(char c) {
        skip_space_and_comma();
        if (*m_pos != c) {
            string error;
            error += "expected '";
            error += c;
            error += "' found '";
            error += *m_pos;
            error += "'";
            if (m_open_objects.empty() == false) {
                error += "during deserialization an instance of ";
                error += m_open_objects.top();
            }
            throw_malformed(error);
        }
        ++m_pos;
    }

    bool try_consume(char c) {
        skip_space_and_comma();
        if (*m_pos == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    inline string::iterator next_delimiter() {
        return find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c) {
                case '(':
                case ')':
                case '{':
                case '}':
                case ' ':
                case ',': {
                    return true;
                }
                default: {
                    return false;
                }
            }
        });
    }

    void integrity_check() {
        if (m_open_objects.empty() || m_obj_had_left_parenthesis.empty()) {
            throw_malformed("missing begin_object()");
        }
        if (   m_obj_had_left_parenthesis.top() == false
            && !isbuiltin(m_open_objects.top())) {
            throw_malformed("expected left parenthesis after "
                            "begin_object call or void value");
        }
    }

 public:

    string_deserializer(string str) : super(&m_addressing), m_str(move(str)) {
        m_pos = m_str.begin();
    }

    string seek_object() {
        skip_space_and_comma();
        // shortcuts for builtin types
        if (*m_pos == '"') {
            return "@str";
        }
        else if (*m_pos == '\'') {
            return "@atom";
        }
        else if (*m_pos == '{') {
            return "@<>";
        }
        // default case
        auto substr_end = next_delimiter();
        if (m_pos == substr_end) {
            throw_malformed("could not seek object type name");
        }
        string result(m_pos, substr_end);
        m_pos = substr_end;
        return result;
    }

    string peek_object() {
        auto pos = m_pos;
        string result = seek_object();
        // restore position in stream
        m_pos = pos;
        return result;
    }

    void begin_object(const string& type_name) {
        m_open_objects.push(type_name);
        //++m_obj_count;
        skip_space_and_comma();
        // suppress leading parenthesis for builtin types
        m_obj_had_left_parenthesis.push(try_consume('('));
        //consume('(');
    }

    void end_object() {
        if (m_open_objects.empty()) {
            throw runtime_error("no object to end");
        }
        if (m_obj_had_left_parenthesis.top() == true) {
            consume(')');
        }
        m_open_objects.pop();
        m_obj_had_left_parenthesis.pop();
        if (m_open_objects.empty()) {
            skip_space_and_comma();
            if (m_pos != m_str.end()) {
                throw_malformed(string("expected end of of string, found: ") + *m_pos);
            }
        }
    }

    size_t begin_sequence() {
        integrity_check();
        consume('{');
        return count(m_pos, find(m_pos, m_str.end(), '}'), ',') + 1;
    }

    void end_sequence() {
        integrity_check();
        consume('}');
    }

    struct from_string {
        const string& str;
        from_string(const string& s) : str(s) { }
        template<typename T>
        void operator()(T& what) {
            istringstream s(str);
            s >> what;
        }
        void operator()(string& what) {
            what = str;
        }
        void operator()(u16string&) { }
        void operator()(u32string&) { }
    };

    primitive_variant read_value(primitive_type ptype) {
        integrity_check();
        if (m_open_objects.top() == "@atom") {
            if (ptype != pt_uint64) {
                throw_malformed("expected read of pt_uint64 after @atom");
            }
            consume('\'');
            auto substr_end = find(m_pos, m_str.end(), '\'');
            if (substr_end == m_str.end()) {
                throw_malformed("couldn't find trailing ' for atom");
            }
            else if ((substr_end - m_pos) > 10) {
                throw_malformed("atom string size > 10");
            }
            string substr(m_pos, substr_end);
            m_pos += substr.size() + 1;
            return detail::atom_val(substr.c_str(), 0xF);
        }
        skip_space_and_comma();
        string::iterator substr_end;
        auto find_if_cond = [] (char c) -> bool {
            switch (c) {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        };
        if (ptype == pt_u8string) {
            if (*m_pos == '"') {
                // skip leading "
                ++m_pos;
                char last_char = '"';
                auto find_if_str_cond = [&last_char] (char c) -> bool {
                    if (c == '"' && last_char != '\\') {
                        return true;
                    }
                    last_char = c;
                    return false;
                };
                substr_end = find_if(m_pos, m_str.end(), find_if_str_cond);
            }
            else {
                substr_end = find_if(m_pos, m_str.end(), find_if_cond);
            }
        }
        else {
            substr_end = find_if(m_pos, m_str.end(), find_if_cond);
        }
        if (substr_end == m_str.end()) {
            throw logic_error("malformed string (unterminated value)");
        }
        string substr(m_pos, substr_end);
        m_pos += substr.size();
        if (ptype == pt_u8string) {
            // skip trailing "
            if (*m_pos != '"') {
                string error_msg;
                error_msg  = "malformed string, expected '\"' found '";
                error_msg += *m_pos;
                error_msg += "'";
                throw logic_error(error_msg);
            }
            ++m_pos;
            // replace '\"' by '"'
            char last_char = ' ';
            auto cond = [&last_char] (char c) -> bool {
                if (c == '"' && last_char == '\\') {
                    return true;
                }
                last_char = c;
                return false;
            };
            string tmp;
            auto sbegin = substr.begin();
            auto send = substr.end();
            for (auto i = find_if(sbegin, send, cond);
                 i != send;
                 i = find_if(i, send, cond)) {
                --i;
                tmp.append(sbegin, i);
                tmp += '"';
                i += 2;
                sbegin = i;
            }
            if (sbegin != substr.begin()) {
                tmp.append(sbegin, send);
            }
            if (!tmp.empty()) {
                substr = move(tmp);
            }
        }
        primitive_variant result(ptype);
        result.apply(from_string(substr));
        return result;
    }

    void read_tuple(size_t size,
                    const primitive_type* begin,
                    primitive_variant* storage) {
        integrity_check();
        consume('{');
        const primitive_type* end = begin + size;
        for ( ; begin != end; ++begin) {
            *storage = move(read_value(*begin));
            ++storage;
        }
        consume('}');
    }

    void read_raw(size_t buf_size, void* vbuf) {
        auto buf = reinterpret_cast<unsigned char*>(vbuf);
        integrity_check();
        skip_space_and_comma();
        auto next_nibble = [&]() -> size_t {
            if (*m_pos == '\0') {
                throw_malformed("unexpected end-of-string");
            }
            char c = *m_pos++;
            if (!isxdigit(c)) {
                throw_malformed("unexpected character, expected [0-9a-f]");
            }
            return static_cast<size_t>(isdigit(c) ? c - '0' : (c - 'a' + 10));
        };
        for (size_t i = 0; i < buf_size; ++i) {
            auto nibble = next_nibble();
            *buf++ = static_cast<unsigned char>((nibble << 4) | next_nibble());
        }
    }

};

} // namespace <anonymous>

object from_string(const string& what) {
    string_deserializer strd(what);
    string uname = strd.peek_object();
    auto utype = uniform_type_info::from(uname);
    if (utype == nullptr) {
        throw logic_error(uname + " is not announced");
    }
    return utype->deserialize(&strd);
}

namespace detail {

string to_string_impl(const void *what, const uniform_type_info *utype) {
    ostringstream osstr;
    string_serializer strs(osstr);
    utype->serialize(what, &strs);
    return osstr.str();
}

} // namespace detail

string to_verbose_string(const exception& e) {
    std::ostringstream oss;
    oss << detail::demangle(typeid(e)) << ": " << e.what();
    return oss.str();
}

} // namespace cppa

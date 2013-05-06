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
 * Free Software Foundation; either version 2.1 of the License,               *
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


#include <map>
#include <cwchar>
#include <limits>
#include <vector>
#include <cstring>
#include <typeinfo>
#include <stdexcept>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_header.hpp"

#include "cppa/util/void_type.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

namespace {

using namespace std;
using namespace cppa;
using namespace detail;

struct platform_int_mapping { const char* name; size_t size; bool is_signed; };

// WARNING: this list is sorted and searched with std::lower_bound;
//          keep ordered when adding elements!
platform_int_mapping platform_dependent_sizes[] = {
    {"char",                sizeof(char),               true},
    {"char16_t",            sizeof(char16_t),           true},
    {"char32_t",            sizeof(char32_t),           true},
    {"int",                 sizeof(int),                true},
    {"long",                sizeof(long),               true},
    {"long int",            sizeof(long int),           true},
    {"long long",           sizeof(long long),          true},
    {"short",               sizeof(short),              true},
    {"short int",           sizeof(short int),          true},
    {"signed char",         sizeof(signed char),        true},
    {"signed int",          sizeof(signed int),         true},
    {"signed long",         sizeof(signed long),        true},
    {"signed long int",     sizeof(signed long int),    true},
    {"signed long long",    sizeof(signed long long),   true},
    {"signed short",        sizeof(signed short),       true},
    {"signed short int",    sizeof(signed short int),   true},
    {"unsigned char",       sizeof(unsigned char),      false},
    {"unsigned int",        sizeof(unsigned int),       false},
    {"unsigned long",       sizeof(unsigned long),      false},
    {"unsigned long int",   sizeof(unsigned long int),  false},
    {"unsigned long long",  sizeof(unsigned long long), false},
    {"unsigned short",      sizeof(unsigned short),     false},
    {"unsigned short int",  sizeof(unsigned short int), false}
};

string map2decorated(const char* name) {
    auto cmp = [](const platform_int_mapping& pim, const char* name) {
        return strcmp(pim.name, name) < 0;
    };
    auto e = end(platform_dependent_sizes);
    auto i = lower_bound(begin(platform_dependent_sizes), e, name, cmp);
    if (i != e && strcmp(i->name, name) == 0) {
        return mapped_int_names[i->size][i->is_signed ? 1 : 0];
    }
    return mapped_name_by_decorated_name(name);
}

class parse_tree {

 public:

    string compile() const {
        string result;
        if (m_volatile) result += "volatile ";
        if (m_const) result += "const ";
        if (!m_template) {
            result += map2decorated(m_name.c_str());
        }
        else {
            string full_name = m_name;
            full_name += "<";
            for (auto& tparam : m_template_parameters) {
                // decorate each single template parameter
                if (full_name.back() != '<') full_name += ",";
                full_name += tparam.compile();
            }
            full_name += ">";
            // decorate full name
            result += map2decorated(full_name.c_str());
        }
        if (m_pointer) result += "*";
        if (m_lvalue_ref) result += "&";
        if (m_rvalue_ref) result += "&&";
        return result;
    }

    template<typename Iterator>
    static vector<parse_tree> parse_tpl_args(Iterator first, Iterator last);

    template<typename Iterator>
    static parse_tree parse(Iterator first, Iterator last) {
        typedef reverse_iterator<Iterator> rev_iter;
        auto sub_first = find(first, last, '<');
        auto sub_last = find(rev_iter(last), rev_iter(first), '>').base() - 1;
        if (sub_last < sub_first) {
            sub_first = sub_last = last;
        }
        auto islegal = [](char c) { return isalnum(c) || c == ':' || c == '_'; };
        vector<string> tokens;
        tokens.push_back("");
        for (auto i = first; i != last;) {
            if (i == sub_first) {
                tokens.push_back("");
                i = sub_last;
            }
            else {
                char c = *i;
                if (islegal(c)) {
                    if (!tokens.back().empty() && !islegal(tokens.back().back())) {
                        tokens.push_back("");
                    }
                    tokens.back() += c;
                }
                else if (c == ' ') {
                    tokens.push_back("");
                }
                else if (c == '&') {
                    if (tokens.back().empty() || tokens.back().back() == '&') {
                        tokens.back() += c;
                    }
                    else {
                        tokens.push_back("&");
                    }
                }
                else if (c == '*') {
                    tokens.push_back("*");
                }
                ++i;
            }
        }
        parse_tree result;
        if (sub_first != sub_last) {
            result.m_template = true;
            result.m_template_parameters = parse_tpl_args(sub_first + 1, sub_last);
        }
        for (auto& token: tokens) {
            if (token == "const") {
                result.m_const = true;
            }
            else if (token == "volatile") {
                result.m_volatile = true;
            }
            else if (token == "&") {
                result.m_lvalue_ref = true;
            }
            else if (token == "&&") {
                result.m_rvalue_ref = true;
            }
            else if (token == "*") {
                result.m_pointer = true;
            }
            else if (token == "class" || token == "struct") {
                // ignored (created by visual c++ compilers)
            }
            else if (!token.empty()) {
                if (!result.m_name.empty()) result.m_name += " ";
                result.m_name += token;
            }
        }
        return result;
    }

 private:

    parse_tree()
    : m_const(false), m_pointer(false), m_volatile(false), m_template(false)
    , m_lvalue_ref(false), m_rvalue_ref(false) { }

    bool m_const;
    bool m_pointer;
    bool m_volatile;
    bool m_template;
    bool m_lvalue_ref;
    bool m_rvalue_ref;

    string m_name;
    vector<parse_tree> m_template_parameters;

};

template<typename Iterator>
vector<parse_tree> parse_tree::parse_tpl_args(Iterator first, Iterator last) {
    vector<parse_tree> result;
    long open_brackets = 0;
    auto i0 = first;
    for (; first != last; ++first) {
        switch (*first) {
            case '<':
                ++open_brackets;
                break;
            case '>':
                --open_brackets;
                break;
            case ',':
                if (open_brackets == 0) {
                    result.push_back(parse(i0, first));
                    i0 = first + 1;
                }
                break;
            default: break;
        }
    }
    result.push_back(parse(i0, first));
    return result;
}

template<size_t RawSize>
void replace_all(string& str, const char (&before)[RawSize], const char* after) {
    // end(before) - 1 points to the null-terminator
    auto i = search(begin(str), end(str), begin(before), end(before) - 1);
    while (i != end(str)) {
        str.replace(i, i + RawSize - 1, after);
        i = search(begin(str), end(str), begin(before), end(before) - 1);
    }
}

const char s_rawan[] = "anonymous namespace";
const char s_an[] = "$";

} // namespace <anonymous>

namespace cppa { namespace detail {

std::string to_uniform_name(const std::string& dname) {
    auto r = parse_tree::parse(begin(dname), end(dname)).compile();
    // replace compiler-dependent "anonmyous namespace" with "@_"
    replace_all(r, s_rawan, s_an);
    return r;
}

std::string to_uniform_name(const std::type_info& tinfo) {
    return to_uniform_name(demangle(tinfo.name()));
}

} } // namespace detail

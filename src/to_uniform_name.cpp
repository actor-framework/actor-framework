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
 * Copyright (C) 2011, 2012                                                   *
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


#include <map>
#include <cwchar>
#include <limits>
#include <vector>
#include <typeinfo>
#include <stdexcept>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/util/void_type.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/network/message_header.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/decorated_names_map.hpp"

namespace {

using namespace std;
using namespace cppa;
using namespace detail;


class parse_tree {

 public:

    string compile() const {
        string result;
        if (m_volatile) result += "volatile ";
        if (m_const) result += "const ";
        if (!m_template) {
            result += dmm->decorate(m_name);
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
            result += dmm->decorate(full_name);
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
    , m_lvalue_ref(false), m_rvalue_ref(false) {
        dmm = singleton_manager::get_decorated_names_map();
    }

    bool m_const;
    bool m_pointer;
    bool m_volatile;
    bool m_template;
    bool m_lvalue_ref;
    bool m_rvalue_ref;
    const decorated_names_map* dmm;

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
const char s_an[] = "@_";

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

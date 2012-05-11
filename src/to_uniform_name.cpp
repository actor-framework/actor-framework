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
#include "cppa/detail/addressed_message.hpp"

namespace {

constexpr const char* mapped_int_names[][2] = {
    { nullptr, nullptr }, // sizeof 0-> invalid
    { "@i8",   "@u8"   }, // sizeof 1 -> signed / unsigned int8
    { "@i16",  "@u16"  }, // sizeof 2 -> signed / unsigned int16
    { nullptr, nullptr }, // sizeof 3-> invalid
    { "@i32",  "@u32"  }, // sizeof 4 -> signed / unsigned int32
    { nullptr, nullptr }, // sizeof 5-> invalid
    { nullptr, nullptr }, // sizeof 6-> invalid
    { nullptr, nullptr }, // sizeof 7-> invalid
    { "@i64",  "@u64"  }  // sizeof 8 -> signed / unsigned int64
};

template<typename T>
constexpr size_t sign_index() {
    static_assert(std::numeric_limits<T>::is_integer, "T is not an integer");
    return std::numeric_limits<T>::is_signed ? 0 : 1;
}

template<typename T>
inline std::string demangled() {
    return cppa::detail::demangle(typeid(T).name());
}

template<typename T>
constexpr const char* mapped_int_name() {
    return mapped_int_names[sizeof(T)][sign_index<T>()];
}

template<typename Iterator>
std::string to_uniform_name_impl(Iterator begin, Iterator end,
                                 bool first_run = false) {
    // all integer type names as uniform representation
    static std::map<std::string, std::string> mapped_demangled_names = {
      // integer types
      { demangled<char>(), mapped_int_name<char>() },
      { demangled<signed char>(), mapped_int_name<signed char>() },
      { demangled<unsigned char>(), mapped_int_name<unsigned char>() },
      { demangled<short>(), mapped_int_name<short>() },
      { demangled<signed short>(), mapped_int_name<signed short>() },
      { demangled<unsigned short>(), mapped_int_name<unsigned short>() },
      { demangled<short int>(), mapped_int_name<short int>() },
      { demangled<signed short int>(), mapped_int_name<signed short int>() },
      { demangled<unsigned short int>(), mapped_int_name<unsigned short int>()},
      { demangled<int>(), mapped_int_name<int>() },
      { demangled<signed int>(), mapped_int_name<signed int>() },
      { demangled<unsigned int>(), mapped_int_name<unsigned int>() },
      { demangled<long int>(), mapped_int_name<long int>() },
      { demangled<signed long int>(), mapped_int_name<signed long int>() },
      { demangled<unsigned long int>(), mapped_int_name<unsigned long int>() },
      { demangled<long>(), mapped_int_name<long>() },
      { demangled<signed long>(), mapped_int_name<signed long>() },
      { demangled<unsigned long>(), mapped_int_name<unsigned long>() },
      { demangled<long long>(), mapped_int_name<long long>() },
      { demangled<signed long long>(), mapped_int_name<signed long long>() },
      { demangled<unsigned long long>(), mapped_int_name<unsigned long long>()},
      { demangled<char16_t>(), mapped_int_name<char16_t>() },
      { demangled<char32_t>(), mapped_int_name<char32_t>() },
      // string types
      { demangled<std::string>(), "@str" },
      { demangled<std::u16string>(), "@u16str" },
      { demangled<std::u32string>(), "@u32str" },
      // cppa types
      { demangled<cppa::atom_value>(), "@atom" },
      { demangled<cppa::util::void_type>(), "@0" },
      { demangled<cppa::any_tuple>(), "@<>" },
      { demangled<cppa::actor_ptr>(), "@actor" },
      { demangled<cppa::group_ptr>(), "@group" },
      { demangled<cppa::channel_ptr>(), "@channel" },
      { demangled<cppa::detail::addressed_message>(), "@msg" },
      { demangled< cppa::intrusive_ptr<cppa::process_information> >(), "@process_info" }
    };

    // check if we could find the whole string in our lookup map
    if (first_run) {
        std::string tmp(begin, end);
        auto i = mapped_demangled_names.find(tmp);
        if (i != mapped_demangled_names.end()) {
            return i->second;
        }
    }

    // does [begin, end) represents an empty string?
    if (begin == end) return "";
    // derived reverse_iterator type
    typedef std::reverse_iterator<Iterator> reverse_iterator;
    // a subsequence [begin', end') within [begin, end)
    typedef std::pair<Iterator, Iterator> subseq;
    std::vector<subseq> substrings;
    // explode string if we got a list of types
    int open_brackets = 0; // counts "open" '<'
    // denotes the begin of a possible subsequence
    Iterator anchor = begin;
    for (Iterator i = begin; i != end; /* i is incemented in the loop */) {
        switch (*i) {

         case '<':
            ++open_brackets;
            ++i;
            break;

         case '>':
            if (--open_brackets < 0) {
                throw std::runtime_error("malformed string");
            }
            ++i;
            break;

         case ',':
            if (open_brackets == 0) {
                substrings.push_back(std::make_pair(anchor, i));
                ++i;
                anchor = i;
            }
            else {
                ++i;
            }
            break;

         default:
            ++i;
            break;

        }
    }
    // call recursively for each list argument
    if (!substrings.empty()) {
        std::string result;
        substrings.push_back(std::make_pair(anchor, end));
        for (const subseq& sstr : substrings) {
            if (!result.empty()) result += ",";
            result += to_uniform_name_impl(sstr.first, sstr.second);
        }
        return result;
    }
    // we didn't got a list, compute unify name
    else {
        // is [begin, end) a template?
        Iterator substr_begin = std::find(begin, end, '<');
        if (substr_begin == end) {
            // not a template, return mapping
            std::string arg(begin, end);
            auto mapped = mapped_demangled_names.find(arg);
            return (mapped == mapped_demangled_names.end()) ? arg : mapped->second;
        }
        // skip leading '<'
        ++substr_begin;
        // find trailing '>'
        Iterator substr_end = std::find(reverse_iterator(end),
                                        reverse_iterator(substr_begin),
                                        '>')
                              // get as an Iterator
                              .base();
        // skip trailing '>'
        --substr_end;
        if (substr_end == substr_begin) {
            throw std::runtime_error("substr_end == substr_begin");
        }
        std::string result;
        // template name (part before leading '<')
        result.append(begin, substr_begin);
        // get mappings of all template parameter(s)
        result += to_uniform_name_impl(substr_begin, substr_end);
        result.append(substr_end, end);
        return result;
    }
}

} // namespace <anonymous>

namespace cppa { namespace detail {

std::string to_uniform_name(const std::string& dname) {
    static std::string an = "(anonymous namespace)";
    static std::string an_replacement = "@_";
    auto r = to_uniform_name_impl(dname.begin(), dname.end(), true);
    // replace all occurrences of an with "@_"
    if (r.size() > an.size()) {
        auto i = std::search(r.begin(), r.end(), an.begin(), an.end());
        while (i != r.end()) {
            auto substr_end = i + an.size();
            r.replace(i, substr_end, an_replacement);
            // next iteration
            i = std::search(r.begin(), r.end(), an.begin(), an.end());
        }
    }
    return r;
}

std::string to_uniform_name(const std::type_info& tinfo) {
    return to_uniform_name(demangle(tinfo.name()));
}

} } // namespace cppa::detail

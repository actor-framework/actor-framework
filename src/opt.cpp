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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#include <iomanip>
#include <algorithm>
#include "cppa/opt.hpp"

using namespace std;
using cppa::placeholders::_x1;

namespace cppa {

detail::opt1_rvalue_builder<true> on_opt1(char short_opt,
                                         string long_opt,
                                         options_description* desc,
                                         string help_text,
                                         string help_group) {
    if (desc) {
        option_info oinf{move(help_text), 1};
        (*desc)[help_group].insert(make_pair(make_pair(short_opt, long_opt), move(oinf)));
    }
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    const char* lhs_str = short_flag_arr;
    string prefix = "--";
    prefix += long_opt;
    prefix += "=";
    function<optional<string> (const string&)> kvp = [prefix](const string& input) -> optional<string> {
        if (equal(begin(prefix), end(prefix), begin(input))) {
            return input.substr(prefix.size());
        }
        else if (equal(begin(prefix) + 1, end(prefix), begin(input))) {
            return input.substr(prefix.size() - 1);
        }
        return none;
    };
    vector<string> opts;
    opts.push_back(lhs_str);
    opts.push_back("--" + long_opt);
    opts.push_back("-" + long_opt);
    return {short_opt, long_opt, on<string, string>().when(_x1.in(opts)), on(kvp)};
}

decltype(on<string>().when(_x1.in(vector<string>())))
on_opt0(char short_opt,
        string long_opt,
        options_description* desc,
        string help_text,
        string help_group) {
    if (desc) {
        option_info oinf{help_text, 0};
        (*desc)[help_group].insert(make_pair(make_pair(short_opt, long_opt), move(oinf)));
    }
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    vector<string> opt_strs = { short_flag_arr };
    opt_strs.push_back("-" + long_opt);
    opt_strs.push_back("--" + move(long_opt));
    return on<string>().when(_x1.in(move(opt_strs)));
}

function<void()> print_desc(options_description* desc, ostream& out) {
    return [&out, desc] {
        if (!desc) return;
        if (desc->empty()) {
            out << "please use '-h' or '--help' for a list "
                   "of available program options\n";
        }
        for (auto& opt_group : *desc) {
            out << opt_group.first << ":\n";
            for (auto& opt : opt_group.second) {
                out << "  ";
                out << setw(40) << left;
                ostringstream tmp;
                auto& names = opt.first;
                if (names.first != '\0') {
                    tmp << "-" << names.first;
                    for (size_t num = 1; num <= opt.second.num_args; ++num) {
                        tmp << " <arg" << num << ">";
                    }
                    tmp << " | ";
                }
                tmp << "--" << names.second;
                if (opt.second.num_args > 0) {
                    tmp << "=<arg1>";
                }
                for (size_t num = 2; num <= opt.second.num_args; ++num) {
                    tmp << ", <arg" << num << ">";
                }
                out << tmp.str() << opt.second.help_text << "\n";
            }
            out << "\n";
        }
    };
}

function<void()> print_desc_and_exit(options_description* desc,
                                          ostream& out,
                                          int exit_reason) {
    auto fun = print_desc(desc, out);
    return [=] {
        fun();
        exit(exit_reason);
    };
}

} // namespace cppa

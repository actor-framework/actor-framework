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
    function<option<string> (const string&)> kvp = [prefix](const string& input) -> option<string> {
        if (equal(begin(prefix), end(prefix), begin(input))) {
            return input.substr(prefix.size());
        }
        else if (equal(begin(prefix) + 1, end(prefix), begin(input))) {
            return input.substr(prefix.size() - 1);
        }
        return {};
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
                    tmp << ",<arg" << num << ">";
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

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


#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <vector>
#include <stdexcept>
#include <algorithm>

template<typename T>
T rd(char const* cstr)
{
    char* endptr = nullptr;
    T result = static_cast<T>(strtol(cstr, &endptr, 10));
    if (endptr == nullptr || *endptr != '\0')
    {
        std::string errstr;
        errstr += "\"";
        errstr += cstr;
        errstr += "\" is not an integer";
        throw std::invalid_argument(errstr);
    }
    return result;
}

int num_cores()
{
    char cbuf[100];
    FILE* cmd = popen("/bin/cat /proc/cpuinfo | /bin/grep processor | /usr/bin/wc -l", "r");
    if (fgets(cbuf, 100, cmd) == 0)
    {
        throw std::runtime_error("cannot determine number of cores");
    }
    pclose(cmd);
    // erase trailing newline
    auto i = std::find(cbuf, cbuf + 100, '\n');
    *i = '\0';
    return rd<int>(cbuf);
}

std::vector<uint64_t> factorize(uint64_t n)
{
    std::vector<uint64_t> result;
    if (n <= 3)
    {
        result.push_back(n);
        return std::move(result);
    }
    uint64_t d = 2;
    while(d < n)
    {
        if((n % d) == 0)
        {
            result.push_back(d);
            n /= d;
        }
        else
        {
            d = (d == 2) ? 3 : (d + 2);
        }
    }
    result.push_back(d);
    return std::move(result);
}

#endif // UTILITY_HPP

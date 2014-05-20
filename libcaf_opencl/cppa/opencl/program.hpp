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
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
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


#ifndef CPPA_OPENCL_PROGRAM_HPP
#define CPPA_OPENCL_PROGRAM_HPP

#include <memory>

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/smart_ptr.hpp"

namespace cppa {
namespace opencl {

template<typename Signature>
class actor_facade;

/**
 * @brief A wrapper for OpenCL's cl_program.
 */
class program {

    template<typename Signature>
    friend class actor_facade;

 public:

    /**
     * @brief Factory method, that creates a cppa::opencl::program
     *        from a given @p kernel_source.
     * @returns A program object.
     */
    static program create(const char* kernel_source, const char* options = nullptr, uint32_t device_id = 0);

 private:

    program(context_ptr context, command_queue_ptr queue, program_ptr program);

    context_ptr m_context;
    program_ptr m_program;
    command_queue_ptr m_queue;

};

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_PROGRAM_HPP

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
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
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


#ifndef CPPA_OPENCL_COMMAND_DISPATCHER_HPP
#define CPPA_OPENCL_COMMAND_DISPATCHER_HPP

#include <atomic>
#include <vector>
#include <functional>

#include "cppa/channel.hpp"
#include "cppa/opencl/global.hpp"
#include "cppa/opencl/command.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/opencl/actor_facade.hpp"
#include "cppa/detail/singleton_mixin.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/intrusive/blocking_single_reader_queue.hpp"

namespace cppa { namespace opencl {

#ifdef CPPA_OPENCL
class command_dispatcher {

    struct worker;

    friend struct worker;

    friend class detail::singleton_manager;

    friend class program;

    friend void enqueue_to_dispatcher(command_dispatcher*, command*);

 public:

    void enqueue();


    template<typename Ret, typename... Args>
    actor_ptr spawn(const program& prog,
                    const std::string& kernel_name) {
        return new actor_facade<Ret (Args...)>(this, prog, kernel_name);
    }

    template<typename Ret, typename... Args>
    actor_ptr spawn(const std::string& kernel_source,
                    const std::string& kernel_name) {
        return spawn<Ret, Args...>(program{kernel_source}, kernel_name);
    }

 private:

    struct device_info {
        unsigned id;
        command_queue_ptr cmd_queue;
        device_ptr dev_id;
        size_t max_itms_per_grp;
        cl_uint max_dim;
        std::vector<size_t> max_itms_per_dim;

        device_info(unsigned id,
                    command_queue_ptr queue,
                    device_ptr device_id,
                    size_t max_itms_per_grp,
                    cl_uint max_dim,
                    std::vector<size_t> max_itms_per_dim)
            : id(id)
            , cmd_queue(queue)
            , dev_id(device_id)
            , max_itms_per_grp(max_itms_per_grp)
            , max_dim(max_dim)
            , max_itms_per_dim(std::move(max_itms_per_dim)) { }
    };

    typedef intrusive::blocking_single_reader_queue<command> job_queue;

    static inline command_dispatcher* create_singleton() {
        return new command_dispatcher;
    }
    void initialize();
    void dispose();
    void destroy();

    std::atomic<unsigned> dev_id_gen;

    job_queue m_job_queue;

    std::thread m_supervisor;

    std::vector<device_info> m_devices;
    context_ptr m_context;

    static void worker_loop(worker*);
    static void supervisor_loop(command_dispatcher *scheduler, job_queue*);
};

#else // CPPA_OPENCL
class command_dispatcher : public detail::singleton_mixin<command_dispatcher> {

};
#endif // CPPA_OPENCL
} } // namespace cppa::opencl

#endif // CPPA_OPENCL_COMMAND_DISPATCHER_HPP

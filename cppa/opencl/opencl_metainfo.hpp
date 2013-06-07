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


#ifndef OPENCL_METAINFO_HPP
#define OPENCL_METAINFO_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "cppa/cppa.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/opencl/actor_facade.hpp"

#include "cppa/detail/singleton_mixin.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace opencl {

//template<typename Ret, typename... Args>
//actor_ptr spawn(const program& prog,
//                const char* kernel_name,
//                const dim_vec& global_dims,
//                const dim_vec& offsets,
//                const dim_vec& local_dims,
//                std::function<option<cow_tuple<typename util::rm_const_and_ref<Args>::type...>>(any_tuple)> map_args,
//                std::function<any_tuple(Ret&)> map_result)
//{
//    return actor_facade<Ret (Args...)>::create(prog,
//                                               kernel_name,
//                                               global_dims,
//                                               offsets,
//                                               local_dims,
//                                               std::move(map_args),
//                                               std::move(map_result));
//}

//template<typename Ret, typename... Args>
//actor_ptr spawn(const program& prog,
//                const char* kernel_name,
//                const dim_vec& global_dims,
//                const dim_vec& offsets = {},
//                const dim_vec& local_dims = {})
//{
//    std::function<option<cow_tuple<typename util::rm_const_and_ref<Args>::type...>>(any_tuple)>
//        map_args = [] (any_tuple msg) {
//        return tuple_cast<typename util::rm_const_and_ref<Args>::type...>(msg);
//    };
//    std::function<any_tuple(Ret&)> map_result = [] (Ret& result) {
//        return make_any_tuple(std::move(result));
//    };
//    return spawn<Ret, Args...>(prog,
//                               kernel_name,
//                               global_dims,
//                               offsets,
//                               local_dims,
//                               std::move(map_args),
//                               std::move(map_result));
//}


class opencl_metainfo {

    friend class program;
    friend class detail::singleton_manager;
    friend command_queue_ptr get_command_queue(uint32_t id);

 public:

 private:

    struct device_info {
        uint32_t id;
        command_queue_ptr cmd_queue;
        device_ptr dev_id;
        size_t max_itms_per_grp;
        cl_uint max_dim;
        dim_vec max_itms_per_dim;

        device_info(unsigned id,
                    command_queue_ptr queue,
                    device_ptr device_id,
                    size_t max_itms_per_grp,
                    cl_uint max_dim,
                    const dim_vec& max_itms_per_dim)
            : id(id)
            , cmd_queue(queue)
            , dev_id(device_id)
            , max_itms_per_grp(max_itms_per_grp)
            , max_dim(max_dim)
            , max_itms_per_dim(max_itms_per_dim) { }
    };

    static inline opencl_metainfo* create_singleton() {
        return new opencl_metainfo;
    }

    void initialize();
    void dispose();
    void destroy();

    std::atomic<uint32_t> dev_id_gen;

    context_ptr m_context;
    std::vector<device_info> m_devices;

};

opencl_metainfo* get_opencl_metainfo();

} } // namespace cppa::opencl

#endif // OPENCL_METAINFO_HPP

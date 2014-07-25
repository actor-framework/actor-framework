/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CPPA_OPENCL_METAINFO_HPP
#define CPPA_OPENCL_METAINFO_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "cppa/cppa.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/opencl/device_info.hpp"
#include "cppa/opencl/actor_facade.hpp"

#include "cppa/detail/singleton_mixin.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {
namespace opencl {

class opencl_metainfo {

    friend class program;
    friend class detail::singleton_manager;
    friend command_queue_ptr get_command_queue(uint32_t id);

 public:

    const std::vector<device_info> get_devices() const;

 private:

    static inline opencl_metainfo* create_singleton() {
        return new opencl_metainfo;
    }

    void initialize();
    void dispose();
    void destroy();

    context_ptr m_context;
    std::vector<device_info> m_devices;

};

opencl_metainfo* get_opencl_metainfo();

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_METAINFO_HPP

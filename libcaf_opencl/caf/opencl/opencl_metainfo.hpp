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

#ifndef CAF_OPENCL_METAINFO_HPP
#define CAF_OPENCL_METAINFO_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "caf/all.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/smart_ptr.hpp"
#include "caf/opencl/device_info.hpp"
#include "caf/opencl/actor_facade.hpp"

//#include "caf/detail/singleton_mixin.hpp"
//#include "caf/detail/singleton_manager.hpp"
#include "caf/detail/singletons.hpp"

namespace caf {
namespace opencl {

class opencl_metainfo : public detail::abstract_singleton {

  friend class program;
  friend class detail::singletons;
  friend command_queue_ptr get_command_queue(uint32_t id);

 public:
  const std::vector<device_info> get_devices() const;

  /**
   * Get opencl_metainfo instance.
   */
  static opencl_metainfo* instance();

 private:
  opencl_metainfo();

  void stop();
  void initialize() override;
  void dispose() override;

  context_ptr m_context;
  std::vector<device_info> m_devices;
};

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_METAINFO_HPP

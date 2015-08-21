/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_OPENCL_PLATFORM_HPP
#define CAF_OPENCL_PLATFORM_HPP

#include <caf/opencl/device.hpp>

namespace caf {
namespace opencl {

class platform {

  friend class program;

public:
  inline const std::vector<device>& get_devices() const;
  inline const std::string& get_name() const;
  inline const std::string& get_vendor() const;
  inline const std::string& get_version() const;
  static platform create(cl_platform_id platform_id, unsigned start_id);

private:
  platform(cl_platform_id platform_id, context_ptr context,
           std::string name, std::string vendor, std::string version,
           std::vector<device> devices);
  static std::string platform_info(cl_platform_id platform_id,
                                   unsigned info_flag);
  cl_platform_id platform_id_;
  context_ptr context_;
  std::string name_;
  std::string vendor_;
  std::string version_;
  std::vector<device> devices_;
};

/******************************************************************************\
 *                 implementation of inline member functions                  *
\******************************************************************************/

inline const std::vector<device>& platform::get_devices() const {
  return devices_;
}

inline const std::string& platform::get_name() const {
  return name_;
}

inline const std::string& platform::get_vendor() const {
  return vendor_;
}

inline const std::string& platform::get_version() const {
  return version_;
}


} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_PLTFORM_HPP

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

 #include "caf/opencl/opencl_err.hpp"

namespace caf {
namespace opencl {

void throwcl(const char* fname, cl_int err) {
  if (err != CL_SUCCESS) {
    std::string errstr = fname;
    errstr += ": ";
    errstr += opencl_error(err);
    throw std::runtime_error(std::move(errstr));
  }
}

void pfn_notify(const char* errinfo, const void*, size_t, void*) {
  CAF_LOG_ERROR("\n##### Error message via pfn_notify #####\n"
                << errinfo <<
                "\n########################################");
  static_cast<void>(errinfo); // remove warning
}

} // namespace opencl
} // namespace caf

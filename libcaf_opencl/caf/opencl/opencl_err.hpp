/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#pragma once


#include "caf/opencl/global.hpp"

#include "caf/logger.hpp"

#define CAF_CLF(funname) #funname , funname

namespace caf {
namespace opencl {

void throwcl(const char* fname, cl_int err);
void CL_CALLBACK pfn_notify(const char* errinfo, const void*, size_t, void*);

// call convention for simply calling a function
template <class F, class... Ts>
void v1callcl(const char* fname, F f, Ts&&... vs) {
  throwcl(fname, f(std::forward<Ts>(vs)...));
}

// call convention for simply calling a function, not using the last argument
template <class F, class... Ts>
void v2callcl(const char* fname, F f, Ts&&... vs) {
  throwcl(fname, f(std::forward<Ts>(vs)..., nullptr));
}

// call convention for simply calling a function, and logging errors
template <class F, class... Ts>
void v3callcl(F f, Ts&&... vs) {
  auto err = f(std::forward<Ts>(vs)...);
  if (err != CL_SUCCESS)
    CAF_LOG_ERROR("error: " << opencl_error(err));
}

// call convention with `result` argument at the end returning `err`, not
// using the second last argument (set to nullptr) nor the one before (set to 0)
template <class R, class F, class... Ts>
R v1get(const char* fname, F f, Ts&&... vs) {
  R result;
  throwcl(fname, f(std::forward<Ts>(vs)..., cl_uint{0}, nullptr, &result));
  return result;
}

// call convention with `err` argument at the end returning `result`
template <class F, class... Ts>
auto v2get(const char* fname, F f, Ts&&... vs)
-> decltype(f(std::forward<Ts>(vs)..., nullptr)) {
  cl_int err;
  auto result = f(std::forward<Ts>(vs)..., &err);
  throwcl(fname, err);
  return result;
}

// call convention with `result` argument at second last position (preceeded by
// its size) followed by an ingored void* argument (nullptr) returning `err`
template <class R, class F, class... Ts>
R v3get(const char* fname, F f, Ts&&... vs) {
  R result;
  throwcl(fname, f(std::forward<Ts>(vs)..., sizeof(R), &result, nullptr));
  return result;
}

} // namespace opencl
} // namespace caf


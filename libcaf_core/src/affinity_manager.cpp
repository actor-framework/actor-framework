/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/affinity/affinity_manager.hpp"

#include <algorithm>
#include <iostream>
#include <set>

#include "caf/actor_system_config.hpp"
#include "caf/affinity/affinity_parser.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/private_thread.hpp"

#ifdef CAF_LINUX
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE
#include <sched.h>
#endif // CAF_LINUX

#ifdef CAF_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define _GNU_SOURCE
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // CAF_WINDOWS

namespace caf {
namespace affinity {

manager::manager(actor_system& sys) : system_(sys) {
  // initialize atomics_ array
  for (auto& a : atomics_)
    a.store(0);
}

void manager::init(actor_system_config& cfg) {
  worker_cores_ = cfg.affinity_worker_cores;
  detached_cores_ = cfg.affinity_detached_cores;
  blocking_cores_ = cfg.affinity_blocking_cores;
  other_cores_ = cfg.affinity_other_cores;

  // workers
  auto& worker_cores = cores_[actor_system::worker_thread];
  parser::parseaffinity(worker_cores_, worker_cores);
  // private (detached thread)
  auto& private_cores = cores_[actor_system::private_thread];
  parser::parseaffinity(detached_cores_, private_cores);
  // blocking
  auto& blocking_cores = cores_[actor_system::blocking_thread];
  parser::parseaffinity(blocking_cores_, blocking_cores);
  // other
  auto& other_cores = cores_[actor_system::other_thread];
  parser::parseaffinity(other_cores_, other_cores);
}

void manager::set_affinity(const actor_system::thread_type tt) {
  CAF_ASSERT(tt < actor_system::no_id);
  auto cores = cores_[tt];
  if (cores.size()) {
    auto& atomics = atomics_[tt];
    size_t id = atomics.fetch_add(1) % cores.size();
    auto Set{cores[id]};
    // Set the affinity of the thread
    set_thread_affinity(0, Set);
  }
}

void manager::set_thread_affinity(int pid, std::set<int> cores) {
  // TODO: add support for FreeBSD with
  // http://netbsd.gw.com/cgi-bin/man-cgi?pthread_setaffinity_np+3+NetBSD-current
#if defined(CAF_LINUX)
  if (cores.size()) {
    // Create a cpu_set_t object representing a set of CPUs.
    // Clear it and mark only CPU i as set.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int const& c : cores) {
      // printf("Thread %p running on core %d\n", (void*)pthread_self(), c);
      CPU_SET(c, &cpuset);
    }
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {
      // perror("sched_setaffinity");
      std::cerr << "Error setting affinity" << std::endl;
    }
  }
#elif defined(CAF_WINDOWS)
  if (cores.size()) {
    HANDLE thread_ptr =
      pid != 0 ? OpenThread(THREAD_ALL_ACCESS, FALSE, pid) : GetCurrentThread();
    // we do not conside the process affinity map
    DWORD_PTR mask = 0;
    for (int const& c : cores) {
      mask |= (static_cast<DWORD_PTR>(1) << c);
    };
    auto ret = SetThreadAffinityMask(thread_ptr, mask);
    if (ret == 0) {
      std::cerr << "Error setting affinity" << std::endl;
    }
    CloseHandle(thread_ptr);
  }
#elif defined(CAF_MACOS)
  std::cerr << "Thread affinity ignored in this platform" << std::endl;
#else
  std::cerr << "Thread affinity not supported in this platform" << std::endl;
#endif
}

void manager::start() {
}
void manager::stop() {
}

actor_system::module::id_t manager::id() const {
  return module::affinity_manager;
}

void* manager::subtype_ptr() {
  return this;
}

} // namespace affinity
} // namespace caf

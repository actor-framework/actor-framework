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

#ifndef CAF_SCHEDULER_PROFILED_COORDINATOR_HPP
#define CAF_SCHEDULER_PROFILED_COORDINATOR_HPP

#include "caf/config.hpp"

#ifdef CAF_MACOS
#include <mach/mach.h>
#else
#include <sys/resource.h>
#endif // CAF_MACOS

#include <cmath>
#include <chrono>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "caf/policy/profiled.hpp"
#include "caf/policy/work_stealing.hpp"

namespace caf {
namespace scheduler {

/**
 * A coordinator which keeps fine-grained profiling state about its workers
 * and their jobs.
 */
template <class Policy = policy::profiled<policy::work_stealing>>
class profiled_coordinator : public coordinator<Policy> {
  using super = coordinator<Policy>;
  using clock_type = std::chrono::high_resolution_clock;

  struct measurement {
   private:
#   ifdef CAF_MACOS
    static std::chrono::microseconds to_usec(const ::time_value_t& tv) {
      return std::chrono::seconds(tv.seconds)
        + std::chrono::microseconds(tv.microseconds);
    }
#   else
    static std::chrono::microseconds to_usec(const ::timeval& tv) {
      return std::chrono::seconds(tv.tv_sec)
        + std::chrono::microseconds(tv.tv_usec);
    }
#   endif // CAF_MACOS

   public:
    static measurement take() {
      auto now = clock_type::now().time_since_epoch();
      measurement m;
      m.time = std::chrono::duration_cast<std::chrono::microseconds>(now);
#     ifdef CAF_MACOS
      auto thread = ::mach_thread_self();
      ::thread_basic_info info;
      auto count = THREAD_BASIC_INFO_COUNT;
      auto result = ::thread_info(thread, THREAD_BASIC_INFO,
                                  reinterpret_cast<thread_info_t>(&info),
                                  &count);
      if (result == KERN_SUCCESS && (info.flags & TH_FLAGS_IDLE) == 0) {
        m.usr = to_usec(info.user_time);
        m.sys = to_usec(info.system_time);
      }
      ::mach_port_deallocate(mach_task_self(), thread);
#     else
      ::rusage ru;
      ::getrusage(RUSAGE_THREAD, &ru);
      m.usr = to_usec(ru.ru_utime);
      m.sys = to_usec(ru.ru_stime);
      m.mem = ru.ru_maxrss;
#     endif // CAF_MACOS
      return m;
    }

    measurement& operator+=(const measurement& other) {
      time += other.time;
      usr += other.usr;
      sys += other.sys;
      mem += other.mem;
      return *this;
    }

    measurement& operator-=(const measurement& other) {
      time -= other.time;
      usr -= other.usr;
      sys -= other.sys;
      mem -= other.mem;
      return *this;
    }

    friend measurement operator+(const measurement& x, const measurement& y) {
      measurement tmp(x);
      tmp += y;
      return tmp;
    }

    friend measurement operator-(const measurement& x, const measurement& y) {
      measurement tmp(x);
      tmp -= y;
      return tmp;
    }

    friend std::ostream& operator<<(std::ostream& out, const measurement& m) {
      out
        << std::setw(15) << m.time.count()
        << std::setw(15) << m.usr.count()
        << std::setw(15) << m.sys.count()
        << std::setw(15) << m.mem;
      return out;
    }

    std::chrono::microseconds time = std::chrono::microseconds::zero();
    std::chrono::microseconds usr = std::chrono::microseconds::zero();
    std::chrono::microseconds sys = std::chrono::microseconds::zero();
    long mem = 0;
  };

 public:
  profiled_coordinator(
    const std::string& filename,
    std::chrono::milliseconds res = std::chrono::milliseconds{1000},
    size_t nw = std::max(std::thread::hardware_concurrency(), 4u),
    size_t mt = std::numeric_limits<size_t>::max())
    : super{nw, mt},
      m_file{filename},
      m_resolution{res},
      m_system_start{std::chrono::system_clock::now()},
      m_clock_start{clock_type::now().time_since_epoch()} {
  }

  void initialize() override {
    super::initialize();
    m_workers.resize(this->num_workers());
    if (! m_file) {
      throw std::runtime_error{"failed to open caf profiler file"};
    }
    m_file.flags(std::ios::left);
    m_file
      << std::setw(21) << "clock"     // UNIX timestamp in microseconds
      << std::setw(10) << "type"      // "actor" or "worker"
      << std::setw(10) << "id"        // ID of the above
      << std::setw(15) << "time"      // duration of this sample (cumulative)
      << std::setw(15) << "usr"       // time spent in user mode (cumulative)
      << std::setw(15) << "sys"       // time spent in kernel model (cumulative)
      << std::setw(15) << "mem"       // used memory (cumulative)
      << std::endl;
  }

  void stop() override {
    super::stop();
    auto now = clock_type::now().time_since_epoch();
    auto wallclock = m_system_start + (now - m_clock_start);
    for (size_t i = 0; i < m_workers.size(); ++i) {
      record(wallclock, "worker", i, m_workers[i].worker);
    }
  }

  // Called by each worker.
  void start_measuring(size_t worker, actor_id job) {
    auto& w = m_workers[worker];
    w.current = job;
    w.job = measurement::take();
  }

  // Called by each worker.
  void stop_measuring(size_t worker, actor_id job) {
    auto m = measurement::take();
    auto& w = m_workers[worker];
    CAF_REQUIRE(job == w.current);
    auto delta = m - w.job;
    // It's not possible that the wallclock timer is less than actual CPU time
    // spent. Due to resolution mismatches of the C++ high-resolution clock and
    // the system timers, this may appear to be the case sometimes. We "fix"
    // this by adjusting the wallclock to the sum of user and system time, so
    // that utilization never exceeds 100%.
    if (delta.time < delta.usr + delta.sys) {
      delta.time = delta.usr + delta.sys;
    }
    w.worker += delta;
    report(job, delta);
    if (m.time - w.last_flush >= m_resolution) {
      w.last_flush = m.time;
      auto wallclock = m_system_start + (m.time - m_clock_start);
      std::lock_guard<std::mutex> file_guard{m_file_mutex};
      record(wallclock, "worker", worker, w.worker);
    }
  }

  // Called by each worker.
  void remove_job(actor_id job) {
    std::lock_guard<std::mutex> job_guard{m_job_mutex};
    auto j = m_jobs.find(job);
    CAF_REQUIRE(j != m_jobs.end());
    if (job != 0) {
      auto now = clock_type::now().time_since_epoch();
      auto wallclock = m_system_start + (now - m_clock_start);
      std::lock_guard<std::mutex> file_guard{m_file_mutex};
      record(wallclock, "actor", job, j->second);
    }
    m_jobs.erase(j);
  }

  struct worker_state {
    actor_id current;
    measurement job;
    measurement worker;
    clock_type::duration last_flush;
  };

 private:
  template <class Time, class Label>
  void record(Time t, Label label, size_t id, measurement const& m) {
    m_file
      << std::setw(21) << t.time_since_epoch().count()
      << std::setw(10) << label
      << std::setw(10) << id
      << m
      << std::endl;
  }

  void report(actor_id const& job, measurement const& m)
  {
    std::lock_guard<std::mutex> job_guard{m_job_mutex};
    m_jobs[job] += m;
    if (m.time - m_last_flush >= m_resolution) {
      m_last_flush = m.time;
      auto now = clock_type::now().time_since_epoch();
      auto wallclock = m_system_start + (now - m_clock_start);
      std::lock_guard<std::mutex> file_guard{m_file_mutex};
      for (auto& j : m_jobs) {
        record(wallclock, "actor", j.first, j.second);
      }
    }
  }

  std::mutex m_job_mutex;
  std::mutex m_file_mutex;
  std::ofstream m_file;
  std::chrono::milliseconds m_resolution;
  std::chrono::system_clock::time_point m_system_start;
  clock_type::duration m_clock_start;
  std::vector<worker_state> m_workers;  // per-worker state
  std::unordered_map<actor_id, measurement> m_jobs;
  clock_type::duration m_last_flush;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_PROFILED_COORDINATOR_HPP

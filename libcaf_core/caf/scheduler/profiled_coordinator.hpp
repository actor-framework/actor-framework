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
#include <mutex>
#include <chrono>
#include <vector>
#include <fstream>
#include <unordered_map>

#include "caf/policy/profiled.hpp"
#include "caf/policy/work_stealing.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace scheduler {

/**
 * A coordinator which keeps fine-grained profiling state about its workers
 * and their jobs.
 */
template <class Policy = policy::profiled<policy::work_stealing>>
class profiled_coordinator : public coordinator<Policy> {
 public:
  using super = coordinator<Policy>;
  using clock_type = std::chrono::high_resolution_clock;

  using usec = std::chrono::microseconds;
  using msec = std::chrono::milliseconds;

  class measurement {
   public:
#   ifdef CAF_MACOS
    static usec to_usec(const ::time_value_t& tv) {
      return std::chrono::seconds(tv.seconds) + usec(tv.microseconds);
    }
#   else
    static usec to_usec(const ::timeval& tv) {
      return std::chrono::seconds(tv.tv_sec) + usec(tv.tv_usec);
    }
#   endif // CAF_MACOS

    static measurement take() {
      auto now = clock_type::now().time_since_epoch();
      measurement m;
      m.time = std::chrono::duration_cast<usec>(now);
#     ifdef CAF_MACOS
      auto tself = ::mach_thread_self();
      ::thread_basic_info info;
      auto count = THREAD_BASIC_INFO_COUNT;
      auto result = ::thread_info(tself, THREAD_BASIC_INFO,
                                  reinterpret_cast<thread_info_t>(&info),
                                  &count);
      if (result == KERN_SUCCESS && (info.flags & TH_FLAGS_IDLE) == 0) {
        m.usr = to_usec(info.user_time);
        m.sys = to_usec(info.system_time);
      }
      ::mach_port_deallocate(mach_task_self(), tself);
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
      using std::setw;
      out << setw(15) << m.time.count()
          << setw(15) << m.usr.count()
          << setw(15) << m.sys.count()
          << setw(15) << m.mem;
      return out;
    }

    usec time = usec::zero();
    usec usr = usec::zero();
    usec sys = usec::zero();
    long mem = 0;
  };

  struct worker_state {
    actor_id current;
    measurement job;
    measurement worker;
    clock_type::duration last_flush;
  };

  profiled_coordinator(const std::string& filename,
                       msec res = msec{1000},
                       size_t nw = std::max(std::thread::hardware_concurrency(),
                                            4u),
                       size_t mt = std::numeric_limits<size_t>::max())
      : super{nw, mt},
        m_file{filename},
        m_resolution{res},
        m_system_start{std::chrono::system_clock::now()},
        m_clock_start{clock_type::now().time_since_epoch()} {
    if (!m_file) {
      throw std::runtime_error{"failed to open CAF profiler file"};
    }
  }

  void initialize() override {
    super::initialize();
    m_worker_states.resize(this->num_workers());
    using std::setw;
    m_file.flags(std::ios::left);
    m_file << setw(21) << "clock"     // UNIX timestamp in microseconds
           << setw(10) << "type"      // "actor" or "worker"
           << setw(10) << "id"        // ID of the above
           << setw(15) << "time"      // duration of this sample (cumulative)
           << setw(15) << "usr"       // time spent in user mode (cumulative)
           << setw(15) << "sys"       // time spent in kernel model (cumulative)
           << setw(15) << "mem"       // used memory (cumulative)
           << std::endl;
  }

  void stop() override {
    CAF_LOG_TRACE("");
    super::stop();
    auto now = clock_type::now().time_since_epoch();
    auto wallclock = m_system_start + (now - m_clock_start);
    for (size_t i = 0; i < m_worker_states.size(); ++i) {
      record(wallclock, "worker", i, m_worker_states[i].worker);
    }
  }

  void start_measuring(size_t worker, actor_id job) {
    auto& w = m_worker_states[worker];
    w.current = job;
    w.job = measurement::take();
  }

  void stop_measuring(size_t worker, actor_id job) {
    auto m = measurement::take();
    auto& w = m_worker_states[worker];
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
      std::lock_guard<std::mutex> file_guard{m_file_mtx};
      record(wallclock, "worker", worker, w.worker);
    }
  }

  void remove_job(actor_id job) {
    std::lock_guard<std::mutex> job_guard{m_job_mtx};
    auto j = m_jobs.find(job);
    CAF_REQUIRE(j != m_jobs.end());
    if (job != 0) {
      auto now = clock_type::now().time_since_epoch();
      auto wallclock = m_system_start + (now - m_clock_start);
      std::lock_guard<std::mutex> file_guard{m_file_mtx};
      record(wallclock, "actor", job, j->second);
    }
    m_jobs.erase(j);
  }

  template <class Time, class Label>
  void record(Time t, Label label, size_t id, measurement const& m) {
    using std::setw;
    m_file << setw(21) << t.time_since_epoch().count()
           << setw(10) << label
           << setw(10) << id
           << m
           << std::endl;
  }

  void report(actor_id const& job, measurement const& m) {
    std::lock_guard<std::mutex> job_guard{m_job_mtx};
    m_jobs[job] += m;
    if (m.time - m_last_flush >= m_resolution) {
      m_last_flush = m.time;
      auto now = clock_type::now().time_since_epoch();
      auto wallclock = m_system_start + (now - m_clock_start);
      std::lock_guard<std::mutex> file_guard{m_file_mtx};
      for (auto& j : m_jobs) {
        record(wallclock, "actor", j.first, j.second);
      }
    }
  }

  std::mutex m_job_mtx;
  std::mutex m_file_mtx;
  std::ofstream m_file;
  msec m_resolution;
  std::chrono::system_clock::time_point m_system_start;
  clock_type::duration m_clock_start;
  std::vector<worker_state> m_worker_states;
  std::unordered_map<actor_id, measurement> m_jobs;
  clock_type::duration m_last_flush;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_PROFILED_COORDINATOR_HPP

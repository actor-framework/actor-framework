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

#pragma once

#include "caf/config.hpp"

#if defined(CAF_MACOS) || defined(CAF_IOS)
#include <mach/mach.h>
#elif defined(CAF_WINDOWS)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <cmath>
#include <mutex>
#include <chrono>
#include <vector>
#include <fstream>
#include <iomanip>
#include <unordered_map>

#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/logger.hpp"
#include "caf/policy/profiled.hpp"
#include "caf/policy/work_stealing.hpp"
#include "caf/scheduler/coordinator.hpp"

namespace caf {
namespace scheduler {

/// A coordinator which keeps fine-grained profiling state about its workers
/// and their jobs.
template <class Policy = policy::profiled<policy::work_stealing>>
class profiled_coordinator : public coordinator<Policy> {
public:
  using super = coordinator<Policy>;
  using clock_type = std::chrono::high_resolution_clock;

  using usec = std::chrono::microseconds;
  using msec = std::chrono::milliseconds;

  class measurement {
  public:
#   if defined(CAF_MACOS) || defined(CAF_IOS)
    static usec to_usec(const ::time_value_t& tv) {
      return std::chrono::seconds(tv.seconds) + usec(tv.microseconds);
    }
#   elif defined(CAF_WINDOWS)
    static usec to_usec(FILETIME const& ft) {
      ULARGE_INTEGER time;
      time.LowPart = ft.dwLowDateTime;
      time.HighPart = ft.dwHighDateTime;

      return std::chrono::seconds(time.QuadPart / 10000000) + usec((time.QuadPart % 10000000) / 10);
    }
#   else
    static usec to_usec(const ::timeval& tv) {
      return std::chrono::seconds(tv.tv_sec) + usec(tv.tv_usec);
    }
#   endif

    static measurement take() {
      auto now = clock_type::now().time_since_epoch();
      measurement m;
      m.runtime = std::chrono::duration_cast<usec>(now);
#     if defined(CAF_MACOS) || defined(CAF_IOS)
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
#     elif defined(CAF_WINDOWS)
      FILETIME creation_time, exit_time, kernel_time, user_time;
      PROCESS_MEMORY_COUNTERS pmc;

      GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
        &kernel_time, &user_time);

      GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

      m.mem = pmc.PeakWorkingSetSize / 1024;
      m.usr = to_usec(user_time);
      m.sys = to_usec(kernel_time);
#     elif defined(CAF_CYGWIN)
      // TODO - decide what to do here instead of zeros
      m.usr = usec::zero();
      m.sys = usec::zero();
      m.mem = 0;
#     else
      ::rusage ru;
      ::getrusage(RUSAGE_THREAD, &ru);
      m.usr = to_usec(ru.ru_utime);
      m.sys = to_usec(ru.ru_stime);
      m.mem = ru.ru_maxrss;
#     endif
      return m;
    }

    measurement& operator+=(const measurement& other) {
      runtime += other.runtime;
      usr += other.usr;
      sys += other.sys;
      mem += other.mem;
      return *this;
    }

    measurement& operator-=(const measurement& other) {
      runtime -= other.runtime;
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
      out << setw(15) << m.runtime.count()
          << setw(15) << m.usr.count()
          << setw(15) << m.sys.count()
          << m.mem;
      return out;
    }

    usec runtime = usec::zero();
    usec usr = usec::zero();
    usec sys = usec::zero();
    long mem = 0;
  };

  struct worker_state {
    actor_id current;
    measurement job;
    measurement worker;
    clock_type::duration last_flush = clock_type::duration::zero();
  };

  profiled_coordinator(actor_system& sys) : super{sys} {
    // nop
  }

  void init(actor_system_config& cfg) override {
    namespace sr = defaults::scheduler;
    super::init(cfg);
    auto fname = get_or(cfg, "scheduler.profiling-output-file",
                        sr::profiling_output_file);
    file_.open(fname);
    if (!file_)
      std::cerr << R"([WARNING] could not open file ")"
                << fname
                << R"(" (no profiler output will be generated))"
                << std::endl;
    auto res = get_or(cfg, "scheduler.profiling-resolution",
                      sr::profiling_resolution);
    resolution_ = std::chrono::duration_cast<msec>(res);
  }

  void start() override {
    clock_start_ = clock_type::now().time_since_epoch();
    super::start();
    worker_states_.resize(this->num_workers());
    using std::setw;
    file_.flags(std::ios::left);
    file_ << setw(21) << "clock"     // UNIX timestamp in microseconds
          << setw(10) << "type"      // "actor" or "worker"
          << setw(10) << "id"        // ID of the above
          << setw(15) << "time"      // duration of this sample (cumulative)
          << setw(15) << "usr"       // time spent in user mode (cumulative)
          << setw(15) << "sys"       // time spent in kernel model (cumulative)
          << "mem"                   // used memory (cumulative)
          << '\n';
  }

  void stop() override {
    CAF_LOG_TRACE("");
    super::stop();
    auto now = clock_type::now().time_since_epoch();
    auto wallclock = system_start_ + (now - clock_start_);
    for (size_t i = 0; i < worker_states_.size(); ++i) {
      record(wallclock, "worker", i, worker_states_[i].worker);
    }
  }

  void start_measuring(size_t worker, actor_id job) {
    auto& w = worker_states_[worker];
    w.current = job;
    w.job = measurement::take();
  }

  void stop_measuring(size_t worker, actor_id job) {
    auto m = measurement::take();
    auto& w = worker_states_[worker];
    CAF_ASSERT(job == w.current);
    auto delta = m - w.job;
    // It's not possible that the wallclock timer is less than actual CPU time
    // spent. Due to resolution mismatches of the C++ high-resolution clock and
    // the system timers, this may appear to be the case sometimes. We "fix"
    // this by adjusting the wallclock to the sum of user and system time, so
    // that utilization never exceeds 100%.
    if (delta.runtime < delta.usr + delta.sys) {
      delta.runtime = delta.usr + delta.sys;
    }
    w.worker += delta;
    report(job, delta);
    if (m.runtime - w.last_flush >= resolution_) {
      w.last_flush = m.runtime;
      auto wallclock = system_start_ + (m.runtime - clock_start_);
      std::lock_guard<std::mutex> file_guard{file_mtx_};
      record(wallclock, "worker", worker, w.worker);
      w.worker = {};
    }
  }

  void remove_job(actor_id job) {
    std::lock_guard<std::mutex> job_guard{job_mtx_};
    auto j = jobs_.find(job);
    if (j != jobs_.end()) {
      if (job != 0) {
        auto now = clock_type::now().time_since_epoch();
        auto wallclock = system_start_ + (now - clock_start_);
        std::lock_guard<std::mutex> file_guard{file_mtx_};
        record(wallclock, "actor", job, j->second);
      }
      jobs_.erase(j);
    }
  }

  template <class Time, class Label>
  void record(Time t, Label label, size_t rec_id, const measurement& m) {
    using std::setw;
    file_ << setw(21) << t.time_since_epoch().count()
           << setw(10) << label
           << setw(10) << rec_id
           << m
           << '\n';
  }

  void report(const actor_id& job, const measurement& m) {
    std::lock_guard<std::mutex> job_guard{job_mtx_};
    jobs_[job] += m;
    if (m.runtime - last_flush_ >= resolution_) {
      last_flush_ = m.runtime;
      auto now = clock_type::now().time_since_epoch();
      auto wallclock = system_start_ + (now - clock_start_);
      std::lock_guard<std::mutex> file_guard{file_mtx_};
      for (auto& j : jobs_) {
        record(wallclock, "actor", j.first, j.second);
        j.second = {};
      }
    }
  }

  std::mutex job_mtx_;
  std::mutex file_mtx_;
  std::ofstream file_;
  msec resolution_;
  std::chrono::system_clock::time_point system_start_;
  clock_type::duration clock_start_;
  std::vector<worker_state> worker_states_;
  std::unordered_map<actor_id, measurement> jobs_;
  clock_type::duration last_flush_ = clock_type::duration::zero();
};

} // namespace scheduler
} // namespace caf


// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/importer/process.hpp"

#include "caf/logger.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/metric_registry.hpp"

// -- detect supported platforms -----------------------------------------------

#if defined(CAF_MACOS) || defined(CAF_LINUX) || defined(CAF_NET_BSD)
#  define CAF_HAS_PROCESS_METRICS
#endif

// -- gauge setup and boilerplate ----------------------------------------------

#ifdef CAF_HAS_PROCESS_METRICS

namespace {

struct sys_stats {
  int64_t rss = 0;
  int64_t vms = 0;
  double cpu_time = 0.0;
  int64_t fds = 0;
};

sys_stats read_sys_stats();

constexpr bool platform_supported_v = true;

template <class CpuPtr, class RssPtr, class VmsPtr, class FdsPtr>
void sys_stats_init(caf::telemetry::metric_registry& reg, RssPtr& rss,
                    VmsPtr& vms, CpuPtr& cpu, FdsPtr& fds) {
  rss = reg.gauge_singleton("process", "resident_memory",
                            "Resident memory size.", "bytes");
  vms = reg.gauge_singleton("process", "virtual_memory", "Virtual memory size.",
                            "bytes");
  cpu = reg.gauge_singleton<double>("process", "cpu",
                                    "Total user and system CPU time spent.",
                                    "seconds", true);
  fds = reg.gauge_singleton("process", "open_fds",
                            "Number of open file descriptors.");
}

void update_impl(caf::telemetry::int_gauge* rss_gauge,
                 caf::telemetry::int_gauge* vms_gauge,
                 caf::telemetry::dbl_gauge* cpu_gauge,
                 caf::telemetry::int_gauge* fds_gauge) {
  auto [rss, vmsize, cpu, fds] = read_sys_stats();
  rss_gauge->value(rss);
  vms_gauge->value(vmsize);
  cpu_gauge->value(cpu);
  fds_gauge->value(fds);
}

} // namespace

#else // CAF_HAS_PROCESS_METRICS

namespace {

constexpr bool platform_supported_v = false;

template <class... Ts>
void sys_stats_init(Ts&&...) {
  // nop
}

template <class... Ts>
void update_impl(Ts&&...) {
  // nop
}

} // namespace

#endif // CAF_HAS_PROCESS_METRICS

// -- macOS-specific scraping logic --------------------------------------------

#ifdef CAF_MACOS

#  include <libproc.h>
#  include <mach/mach.h>
#  include <mach/task.h>
#  include <sys/resource.h>
#  include <unistd.h>

namespace {

sys_stats read_sys_stats() {
  sys_stats result;
  // Fetch memory usage.
  {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count)
        == KERN_SUCCESS) {
      result.rss = info.resident_size;
      result.vms = info.virtual_size;
    }
  }
  // Fetch CPU time.
  {
    task_thread_times_info info;
    mach_msg_type_number_t count = TASK_THREAD_TIMES_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO,
                  reinterpret_cast<task_info_t>(&info), &count)
        == KERN_SUCCESS) {
      // Round to milliseconds.
      result.cpu_time += info.user_time.seconds;
      result.cpu_time += ceil(info.user_time.microseconds / 1000.0) / 1000.0;
      result.cpu_time += info.system_time.seconds;
      result.cpu_time += ceil(info.system_time.microseconds / 1000.0) / 1000.0;
    }
  }
  // Fetch open file handles.
  {
    // proc_pidinfo is undocumented, but this is what lsof also uses.
    auto suggested_buf_size = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0,
                                           nullptr, 0);
    if (suggested_buf_size > 0) {
      auto buf_size = suggested_buf_size;
      auto buf = malloc(buf_size); // TODO: could be thread-local
      auto res = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, buf, buf_size);
      free(buf);
      if (res > 0)
        result.fds = res / sizeof(proc_fdinfo);
    }
  }
  return result;
}

} // namespace

#endif // CAF_MACOS

// -- helper for caching the result of a syscall -------------------------------

#if defined(CAF_LINUX) || defined(CAF_NET_BSD)

#  include <dirent.h>
#  include <unistd.h>

int64_t count_entries_in_directory(const char* path) {
  int64_t result = 0;
  if (auto dptr = opendir(path); dptr != nullptr) {
    for (auto entry = readdir(dptr); entry != nullptr; entry = readdir(dptr)) {
      auto fname = entry->d_name;
      if (strcmp(".", fname) != 0 && strcmp("..", fname) != 0)
        ++result;
    }
    closedir(dptr);
  }
  return result;
}

/// Caches the result from a `sysconf` call in a cache variable to avoid
/// frequent syscalls. Sets `cache_var` to -1 in case of an error. Initially,
/// `cache_var` must be 0 and we assume a successful syscall would always return
/// some value > 0. If `cache_var` is > 0 then this function simply returns the
/// cached value directly.
bool load_system_setting(std::atomic<long>& cache_var, long& var, int name,
                         [[maybe_unused]] const char* pretty_name) {
  var = cache_var.load();
  switch (var) {
    case -1:
      return false;
    case 0:
      var = sysconf(name);
      if (var <= 0) {
        CAF_LOG_ERROR("failed to read" << pretty_name << "from sysconf");
        var = -1;
        cache_var = var;
        return false;
      } else {
        cache_var = var;
        return true;
      }
    default:
      return true;
  }
}

#  define TRY_LOAD(varname, confname)                                          \
    load_system_setting(global_##varname, varname, confname, #confname)

#endif

// -- Linux-specific scraping logic --------------------------------------------

#ifdef CAF_LINUX

#  include <dirent.h>

#  include <cstdio>

namespace {

std::atomic<long> global_ticks_per_second;

std::atomic<long> global_page_size;

sys_stats read_sys_stats() {
  sys_stats result;
  long ticks_per_second = 0;
  long page_size = 0;
  if (!TRY_LOAD(ticks_per_second, _SC_CLK_TCK)
      || !TRY_LOAD(page_size, _SC_PAGE_SIZE))
    return result;
  if (auto f = fopen("/proc/self/stat", "r")) {
    long unsigned utime_ticks = 0;
    long unsigned stime_ticks = 0;
    long unsigned vmsize_bytes = 0;
    long rss_pages = 0;
    auto rd = fscanf(f,
                     "%*d " //  1. PID
                     "%*s " //  2. Executable
                     "%*c " //  3. State
                     "%*d " //  4. Parent PID
                     "%*d " //  5. Process group ID
                     "%*d " //  6. Session ID
                     "%*d " //  7. Controlling terminal
                     "%*d " //  8. Foreground process group ID
                     "%*u " //  9. Flags
                     "%*u " // 10. Number of minor faults
                     "%*u " // 11. Number of minor faults of waited-for children
                     "%*u " // 12. Number of major faults
                     "%*u " // 13. Number of major faults of waited-for children
                     "%lu " // 14. CPU user time in ticks
                     "%lu " // 15. CPU kernel time in ticks
                     "%*d " // 16. CPU user time of waited-for children
                     "%*d " // 17. CPU kernel time of waited-for children
                     "%*d " // 18. Priority
                     "%*d " // 19. Nice value
                     "%*d " // 20. Num threads
                     "%*d " // 21. Obsolete since 2.6
                     "%*u " // 22. Time the process started after system boot
                     "%lu " // 23. Virtual memory size in bytes
                     "%ld", // 24. Resident set size in pages
                     &utime_ticks, &stime_ticks, &vmsize_bytes, &rss_pages);
    fclose(f);
    if (rd != 4) {
      CAF_LOG_ERROR("failed to read content of /proc/self/stat");
      global_ticks_per_second = -1;
      global_page_size = -1;
      return result;
    }
    result.rss = static_cast<int64_t>(rss_pages) * page_size;
    result.vms = static_cast<int64_t>(vmsize_bytes);
    result.cpu_time = utime_ticks;
    result.cpu_time += stime_ticks;
    result.cpu_time /= ticks_per_second;
  }
  result.fds = count_entries_in_directory("/proc/self/fd");
  return result;
}

} // namespace

// -- NetBSD-specific scraping logic -------------------------------------------

#elif defined(CAF_NET_BSD)

#  include <sys/sysctl.h>

namespace {

std::atomic<long> global_page_size;

sys_stats read_sys_stats() {
  auto result = sys_stats;
  auto kip2 = kinfo_proc2{};
  auto kip2_size = sizeof(kip2);
  int mib[6] = {
    CTL_KERN, KERN_PROC2, KERN_PROC_PID, getpid(), static_cast<int>(kip2_size),
    1,
  };
  long page_size = 0;
  if (!TRY_LOAD(page_size, _SC_PAGE_SIZE))
    return result;
  if (sysctl(mib, 6, &kip2, &kip2_size, nullptr, size_t{0}) != 0) {
    CAF_LOG_ERROR("failed to call sysctl in read_sys_stats");
    global_page_size = -1;
    return result;
  }
  result.rss = static_cast<int64_t>(kip2.p_vm_rssize) * page_size;
  result.vms = static_cast<int64_t>(kip2.p_vm_vsize) * page_size;
  result.cpu_time = kip2.p_rtime_sec;
  result.cpu_time += static_cast<double>(kip2.p_rtime_usec) / 1000000;
  return result;
}

} // namespace

#endif // CAF_LINUX

namespace caf::telemetry::importer {

process::process(metric_registry& reg) {
  sys_stats_init(reg, rss_, vms_, cpu_, fds_);
}

bool process::platform_supported() noexcept {
  return platform_supported_v;
}

void process::update() {
  update_impl(rss_, vms_, cpu_, fds_);
}

} // namespace caf::telemetry::importer

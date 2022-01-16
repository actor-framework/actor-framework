// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/importer/process.hpp"

#include "caf/logger.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/metric_registry.hpp"

// -- detect supported platforms -----------------------------------------------

#if defined(CAF_MACOS) || defined(CAF_LINUX) || defined(CAF_NETBSD)
#  define CAF_HAS_PROCESS_METRICS
#endif

// -- gauge setup and boilerplate ----------------------------------------------

#ifdef CAF_HAS_PROCESS_METRICS

namespace {

struct sys_stats {
  int64_t rss;
  int64_t vms;
  double cpu_time;
};

sys_stats read_sys_stats();

static constexpr bool platform_supported_v = true;

template <class CpuPtr, class RssPtr, class VmsPtr>
void sys_stats_init(caf::telemetry::metric_registry& reg, RssPtr& rss,
                    VmsPtr& vms, CpuPtr& cpu) {
  rss = reg.gauge_singleton("process", "resident_memory",
                            "Resident memory size.", "bytes");
  vms = reg.gauge_singleton("process", "virtual_memory", "Virtual memory size.",
                            "bytes");
  cpu = reg.gauge_singleton<double>("process", "cpu",
                                    "Total user and system CPU time spent.",
                                    "seconds", true);
}

void update_impl(caf::telemetry::int_gauge* rss_gauge,
                 caf::telemetry::int_gauge* vms_gauge,
                 caf::telemetry::dbl_gauge* cpu_gauge) {
  auto [rss, vmsize, cpu] = read_sys_stats();
  rss_gauge->value(rss);
  vms_gauge->value(vmsize);
  cpu_gauge->value(cpu);
}

} // namespace

#else // CAF_HAS_PROCESS_METRICS

namespace {

static constexpr bool platform_supported_v = false;

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

#  include <mach/mach.h>
#  include <mach/task.h>
#  include <sys/resource.h>

namespace {

sys_stats read_sys_stats() {
  sys_stats result{0, 0, 0};
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
  return result;
}

} // namespace

#endif // CAF_MACOS

// -- Linux-specific scraping logic --------------------------------------------

#ifdef CAF_LINUX

#  include <cstdio>
#  include <unistd.h>

namespace {

std::atomic<long> global_ticks_per_second;

std::atomic<long> global_page_size;

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
      }
      cache_var = var;
      return true;
    default:
      return true;
  }
}

#  define TRY_LOAD(varname, confname)                                          \
    load_system_setting(global_##varname, varname, confname, #confname)

sys_stats read_sys_stats() {
  sys_stats result{0, 0, 0};
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
  return result;
}

} // namespace

#endif // CAF_LINUX

#if defined(CAF_NETBSD)

#include <sys/sysctl.h>
#include <unistd.h>

namespace {

sys_stats read_sys_stats() {
  sys_stats result{0, 0, 0};
  int mib[6];
  struct kinfo_proc2 kip2;
  size_t kip2_size = sizeof(kip2);

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC2;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid();
  mib[4] = kip2_size;
  mib[5] = 1;

  auto page_size = sysconf(_SC_PAGE_SIZE);
  if (page_size == -1) {
    CAF_LOG_ERROR(
      "getting _SC_PAGE_SIZE from sysconf failed in read_sys_stats");
    return result;
  }


  if (sysctl(mib, 6, &kip2, &kip2_size, NULL, (size_t)0)) {
    CAF_LOG_ERROR("sysctl failed in read_sys_stats");
    return result;
  }

  result.rss = static_cast<int64_t>(kip2.p_vm_rssize) * page_size;
  result.vms = static_cast<int64_t>(kip2.p_vm_vsize) * page_size;
  result.cpu_time = kip2.p_rtime_sec;
  result.cpu_time += static_cast<double>(kip2.p_rtime_usec) / 1000000;

  return result;
}

} // namepace

#endif // CAF_NETBSD

namespace caf::telemetry::importer {

process::process(metric_registry& reg) {
  sys_stats_init(reg, rss_, vms_, cpu_);
}

bool process::platform_supported() noexcept {
  return platform_supported_v;
}

void process::update() {
  update_impl(rss_, vms_, cpu_);
}

} // namespace caf::telemetry::importer

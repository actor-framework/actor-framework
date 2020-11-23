/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/prometheus_broker.hpp"

#include "caf/span.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/string_view.hpp"
#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"

namespace {

struct [[maybe_unused]] sys_stats {
  int64_t rss;
  int64_t vmsize;
  double cpu_time;
};

} // namespace

#ifdef CAF_MACOS
#  include <mach/mach.h>
#  include <mach/task.h>
#  include <sys/resource.h>
#  define HAS_PROCESS_METRICS
namespace {

sys_stats read_sys_stats() {
  sys_stats result{0, 0, 0};
  // Fetch memory usage.
  {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info,
                  &count)
        == KERN_SUCCESS) {
      result.rss = info.resident_size;
      result.vmsize = info.virtual_size;
    }
  }
  // Fetch CPU time.
  {
    task_thread_times_info info;
    mach_msg_type_number_t count = TASK_THREAD_TIMES_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t) &info,
                  &count)
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

#ifdef CAF_LINUX
#  include <cstdio>
#  include <unistd.h>
#  define HAS_PROCESS_METRICS
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
    result.vmsize = static_cast<int64_t>(vmsize_bytes);
    result.cpu_time = utime_ticks;
    result.cpu_time += stime_ticks;
    result.cpu_time /= ticks_per_second;
  }
  return result;
}

} // namespace
#endif // CAF_LINUX

namespace caf::detail {

namespace {

// Cap incoming HTTP requests.
constexpr size_t max_request_size = 512 * 1024;

// HTTP response for requests that exceed the size limit.
constexpr string_view request_too_large
  = "HTTP/1.1 413 Request Entity Too Large\r\n"
    "Connection: Closed\r\n\r\n";

// HTTP response for requests that aren't "GET /metrics HTTP/1.1".
constexpr string_view request_not_supported = "HTTP/1.1 501 Not Implemented\r\n"
                                              "Connection: Closed\r\n\r\n";

// HTTP header when sending a payload.
constexpr string_view request_ok = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Connection: Closed\r\n\r\n";

} // namespace

prometheus_broker::prometheus_broker(actor_config& cfg) : io::broker(cfg) {
#ifdef HAS_PROCESS_METRICS
  using telemetry::dbl_gauge;
  using telemetry::int_gauge;
  auto& reg = system().metrics();
  cpu_time_ = reg.gauge_singleton<double>(
    "process", "cpu", "Total user and system CPU time spent.", "seconds", true);
  mem_size_ = reg.gauge_singleton("process", "resident_memory",
                                  "Resident memory size.", "bytes");
  virt_mem_size_ = reg.gauge_singleton("process", "virtual_memory",
                                       "Virtual memory size.", "bytes");
#endif // HAS_PROCESS_METRICS
}

prometheus_broker::prometheus_broker(actor_config& cfg, io::doorman_ptr ptr)
  : prometheus_broker(cfg) {
  add_doorman(std::move(ptr));
}

prometheus_broker::~prometheus_broker() {
  // nop
}

const char* prometheus_broker::name() const {
  return "caf.system.prometheus-broker";
}

bool prometheus_broker::has_process_metrics() noexcept {
#ifdef HAS_PROCESS_METRICS
  return true;
#else  // HAS_PROCESS_METRICS
  return false;
#endif // HAS_PROCESS_METRICS
}

behavior prometheus_broker::make_behavior() {
  return {
    [=](const io::new_data_msg& msg) {
      auto& req = requests_[msg.handle];
      if (req.size() + msg.buf.size() > max_request_size) {
        write(msg.handle, as_bytes(make_span(request_too_large)));
        flush(msg.handle);
        close(msg.handle);
        return;
      }
      req.insert(req.end(), msg.buf.begin(), msg.buf.end());
      auto req_str = string_view{reinterpret_cast<char*>(req.data()),
                                 req.size()};
      // Stop here if the header isn't complete yet.
      if (!ends_with(req_str, "\r\n\r\n"))
        return;
      // We only check whether it's a GET request for /metrics for HTTP 1.x.
      // Everything else, we ignore for now.
      if (!starts_with(req_str, "GET /metrics HTTP/1.")) {
        write(msg.handle, as_bytes(make_span(request_not_supported)));
        flush(msg.handle);
        close(msg.handle);
        return;
      }
      // Collect metrics, ship response, and close.
      scrape();
      auto hdr = as_bytes(make_span(request_ok));
      auto text = collector_.collect_from(system().metrics());
      auto payload = as_bytes(make_span(text));
      auto& dst = wr_buf(msg.handle);
      dst.insert(dst.end(), hdr.begin(), hdr.end());
      dst.insert(dst.end(), payload.begin(), payload.end());
      flush(msg.handle);
      close(msg.handle);
    },
    [=](const io::new_connection_msg& msg) {
      // Pre-allocate buffer for maximum request size.
      auto& req = requests_[msg.handle];
      req.reserve(512 * 1024);
      configure_read(msg.handle, io::receive_policy::at_most(1024));
    },
    [=](const io::connection_closed_msg& msg) {
      requests_.erase(msg.handle);
      if (num_connections() + num_doormen() == 0)
        quit();
    },
    [=](const io::acceptor_closed_msg&) {
      CAF_LOG_ERROR("Prometheus Broker lost its acceptor!");
      if (num_connections() + num_doormen() == 0)
        quit();
    },
  };
}

void prometheus_broker::scrape() {
#ifdef HAS_PROCESS_METRICS
  // Collect system metrics at most once per second.
  auto now = time(NULL);
  if (last_scrape_ >= now)
    return;
  last_scrape_ = now;
  auto [rss, vmsize, cpu_time] = read_sys_stats();
  mem_size_->value(rss);
  virt_mem_size_->value(vmsize);
  cpu_time_->value(cpu_time);
#endif // HAS_PROCESS_METRICS
}

} // namespace caf::detail

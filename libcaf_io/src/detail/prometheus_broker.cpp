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

#ifdef CAF_MACOS
#  include <mach/mach.h>
#  include <mach/task.h>
#  include <sys/resource.h>
#  define HAS_PROCESS_METRICS
namespace {

std::pair<int64_t, int64_t> get_mem_usage() {
  mach_task_basic_info info;
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info,
                &count)
      != KERN_SUCCESS) {
    return {0, 0};
  }
  return {static_cast<int64_t>(info.resident_size),
          static_cast<int64_t>(info.virtual_size)};
}

double get_cpu_time() {
  task_thread_times_info info;
  mach_msg_type_number_t count = TASK_THREAD_TIMES_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t) &info,
                &count)
      != KERN_SUCCESS)
    return 0;
  // Round to milliseconds.
  double result = 0;
  result += info.user_time.seconds;
  result += ceil(info.user_time.microseconds / 1000.0) / 1000.0;
  result += info.system_time.seconds;
  result += ceil(info.system_time.microseconds / 1000.0) / 1000.0;
  return result;
}

} // namespace
#endif // CAF_MACOS

#ifdef CAF_LINUX
#  include <sys/resource.h>
#  define HAS_PROCESS_METRICS
std::pair<int64_t, int64_t> get_mem_usage() {
  return {0, 0};
}
int64_t get_cpu_time() {
  rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    return 0;
  return static_cast<int64_t>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec);
}
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
  auto& reg = system().telemetry();
  cpu_time_ = reg.singleton<dbl_gauge>(
    "process", "cpu", "Total user and system CPU time spent.", "seconds", true);
  mem_size_ = reg.singleton<int_gauge>("process", "resident_memory",
                                       "Resident memory size.", "bytes");
  virt_mem_size_ = reg.singleton<int_gauge>("process", "virtual_memory",
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
  return "prometheus-broker";
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
      auto text = collector_.collect_from(system().telemetry());
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
      // No further action required other than cleaning up the state.
      requests_.erase(msg.handle);
    },
    [=](const io::acceptor_closed_msg&) {
      // Shoud not happen.
      quit(sec::socket_operation_failed);
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
  cpu_time_->value(get_cpu_time());
  auto [res, virt] = get_mem_usage();
  mem_size_->value(res);
  virt_mem_size_->value(virt);
#endif // HAS_PROCESS_METRICS
}

} // namespace caf::detail

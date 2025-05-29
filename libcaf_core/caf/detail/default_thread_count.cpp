// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_thread_count.hpp"

#include "caf/config.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <thread>

namespace caf::detail {

size_t default_thread_count() {
#ifdef CAF_LINUX
  const auto hardware_thread_count = std::thread::hardware_concurrency();
  std::ifstream cpu_max{"/sys/fs/cgroup/cpu.max"};
  std::ifstream cfs_quota{"/sys/fs/cgroup/cpu/cpu.cfs_quota_us"};
  std::ifstream cfs_period{"/sys/fs/cgroup/cpu/cpu.cfs_period_us"};
  int64_t quota, period;
  if (cpu_max.is_open()) {
    // cgroup v2
    // https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html
    std::string buffer;
    if (!std::getline(cpu_max, buffer))
      return hardware_thread_count;
    std::istringstream iss_cpu{buffer};
    if (!(iss_cpu >> quota >> period))
      return hardware_thread_count;
  } else if (cfs_quota.is_open() && cfs_period.is_open()) {
    // cgroup v1
    // https://www.kernel.org/doc/html/latest/scheduler/sched-bwc.html#management
    const auto get_cgroup_value
      = [](std::ifstream& file) -> std::optional<int64_t> {
      std::string line;
      if (!std::getline(file, line))
        return std::nullopt;
      std::istringstream iss{line};
      int64_t value;
      if (!(iss >> value))
        return std::nullopt;
      return value;
    };
    auto maybe_quota = get_cgroup_value(cfs_quota);
    auto maybe_period = get_cgroup_value(cfs_period);
    if (!maybe_quota || !maybe_period)
      return hardware_thread_count;
    quota = *maybe_quota;
    period = *maybe_period;
  } else {
    // No cgroup quota
    return hardware_thread_count;
  }
  if (quota > 0 && period > 0)
    return int64_t(std::ceil(double(quota) / double(period)));
  else
    return hardware_thread_count;
#else
  return std::max(std::thread::hardware_concurrency(), 4u);
#endif
}

} // namespace caf::detail

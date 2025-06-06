// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_thread_count.hpp"

#include "caf/config.hpp"

#include <algorithm>
#include <thread>

#ifdef CAF_LINUX
#  include <cmath>
#  include <fstream>
#  include <optional>
#  include <sstream>
#endif

namespace caf::detail {

namespace {

// The minimum number of threads to use in a single actor system. Even on
// low-concurrency systems, we want to use at least 4 threads to make sure
// that the system is responsive.
static constexpr auto min_concurrency = 4u;

} // namespace

// For cgroup v1, see:
// https://www.kernel.org/doc/html/latest/scheduler/sched-bwc.html#management
//
// For cgroup v2, see:
// https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html

size_t default_thread_count() {
  auto fallback = std::max(std::thread::hardware_concurrency(),
                           min_concurrency);
#ifdef CAF_LINUX
  auto quota = size_t{0};
  auto period = size_t{0};
  std::ifstream cpu_max{"/sys/fs/cgroup/cpu.max"};
  if (cpu_max.is_open()) { // cgroup v2
    std::string buffer;
    if (!std::getline(cpu_max, buffer))
      return fallback;
    std::istringstream iss_cpu{buffer};
    if (!(iss_cpu >> quota >> period))
      return fallback;
  } else { // cgroup v1
    std::ifstream cfs_quota{"/sys/fs/cgroup/cpu/cpu.cfs_quota_us"};
    std::ifstream cfs_period{"/sys/fs/cgroup/cpu/cpu.cfs_period_us"};
    if (cfs_quota.is_open() && cfs_period.is_open()) {
      auto get_cgroup_value = [](std::ifstream& file) -> std::optional<size_t> {
        std::string line;
        if (!std::getline(file, line))
          return std::nullopt;
        std::istringstream iss{line};
        auto value = size_t{0};
        if (!(iss >> value))
          return std::nullopt;
        return value;
      };
      auto maybe_quota = get_cgroup_value(cfs_quota);
      auto maybe_period = get_cgroup_value(cfs_period);
      if (maybe_quota && maybe_period) {
        quota = *maybe_quota;
        period = *maybe_period;
      }
    }
  }
  if (quota > 0 && period > 0) {
    auto cpu_ratio = static_cast<double>(quota) / static_cast<double>(period);
    if (auto result = std::ceil(cpu_ratio); result > 0) {
      return std::max(static_cast<size_t>(result), size_t{min_concurrency});
    }
  }
#endif
  return fallback;
}

} // namespace caf::detail

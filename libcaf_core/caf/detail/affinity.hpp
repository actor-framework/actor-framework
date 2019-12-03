#pragma once

#include <set>
#include <vector>

namespace caf::detail {

using core_group = std::set<int>;
using core_groups = std::vector<core_group>;

void set_current_thread_affinity(const core_group& cores);
void set_thread_affinity(const int pid, const core_group& cores);

} // namespace caf::detail
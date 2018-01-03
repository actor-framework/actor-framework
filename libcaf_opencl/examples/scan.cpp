/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/


#include <vector>
#include <random>
#include <iomanip>
#include <numeric>
#include <iostream>

#include "caf/all.hpp"
#include "caf/opencl/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::opencl;

using caf::detail::limited_vector;

namespace {

using uval = unsigned;
using uvec = std::vector<uval>;
using uref = mem_ref<uval>;

constexpr const size_t problem_size = 23;

constexpr const char* kernel_name_1 = "phase_1";
constexpr const char* kernel_name_2 = "phase_2";
constexpr const char* kernel_name_3 = "phase_3";

// opencl kernel, exclusive scan
// last parameter is, by convention, the output parameter
constexpr const char* kernel_source = R"__(
/// Global exclusive scan, phase 1. From:
/// - http://http.developer.nvidia.com/GPUGems3/gpugems3_ch39.html
kernel void phase_1(global uint* restrict data,
                    global uint* restrict increments,
                    local uint* tmp, uint len) {
  const uint thread = get_local_id(0);
  const uint block = get_group_id(0);
  const uint threads_per_block = get_local_size(0);
  const uint elements_per_block = threads_per_block * 2;
  const uint global_offset = block * elements_per_block;
  const uint n = elements_per_block;
  uint offset = 1;
  // A (2 lines) --> load input into shared memory
  tmp[2 * thread] = (global_offset + (2 * thread) < len)
                  ? data[global_offset + (2 * thread)] : 0;
  tmp[2 * thread + 1] = (global_offset + (2 * thread + 1) < len)
                      ? data[global_offset + (2 * thread + 1)] : 0;
  // build sum in place up the tree
  for (uint d = n >> 1; d > 0; d >>= 1) {
    barrier(CLK_LOCAL_MEM_FENCE);
    if (thread < d) {
      // B (2 lines)
      int ai = offset * (2 * thread + 1) - 1;
      int bi = offset * (2 * thread + 2) - 1;
      tmp[bi] += tmp[ai];
    }
    offset *= 2;
  }
  // C (2 lines) --> clear the last element
  if (thread == 0) {
    increments[block] = tmp[n - 1];
    tmp[n - 1] = 0;
  }
  // traverse down tree & build scan
  for (uint d = 1; d < n; d *= 2) {
    offset >>= 1;
    barrier(CLK_LOCAL_MEM_FENCE);
    if (thread < d) {
      // D (2 lines)
      int ai = offset * (2 * thread + 1) - 1;
      int bi = offset * (2 * thread + 2) - 1;
      uint t = tmp[ai];
      tmp[ai] = tmp[bi];
      tmp[bi] += t;
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  // E (2 line) --> write results to device memory
  if (global_offset + (2 * thread) < len)
    data[global_offset + (2 * thread)] = tmp[2 * thread];
  if (global_offset + (2 * thread + 1) < len)
    data[global_offset + (2 * thread + 1)] = tmp[2 * thread + 1];
}

/// Global exclusive scan, phase 2.
kernel void phase_2(global uint* restrict data, // not used ...
                    global uint* restrict increments,
                    uint len) {
  local uint tmp[2048];
  uint thread = get_local_id(0);
  uint offset = 1;
  const uint n = 2048;
  // A (2 lines) --> load input into shared memory
  tmp[2 * thread] = (2 * thread < len) ? increments[2 * thread] : 0;
  tmp[2 * thread + 1] = (2 * thread + 1 < len) ? increments[2 * thread + 1] : 0;
  // build sum in place up the tree
  for (uint d = n >> 1; d > 0; d >>= 1) {
    barrier(CLK_LOCAL_MEM_FENCE);
    if (thread < d) {
      // B (2 lines)
      int ai = offset * (2 * thread + 1) - 1;
      int bi = offset * (2 * thread + 2) - 1;
      tmp[bi] += tmp[ai];
    }
    offset *= 2;
  }
  // C (2 lines) --> clear the last element
  if (thread == 0)
    tmp[n - 1] = 0;
  // traverse down tree & build scan
  for (uint d = 1; d < n; d *= 2) {
    offset >>= 1;
    barrier(CLK_LOCAL_MEM_FENCE);
    if (thread < d) {
      // D (2 lines)
      int ai = offset * (2 * thread + 1) - 1;
      int bi = offset * (2 * thread + 2) - 1;
      uint t = tmp[ai];
      tmp[ai] = tmp[bi];
      tmp[bi] += t;
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  // E (2 line) --> write results to device memory
  if (2 * thread < len) increments[2 * thread] = tmp[2 * thread];
  if (2 * thread + 1 < len) increments[2 * thread + 1] = tmp[2 * thread + 1];
}

kernel void phase_3(global uint* restrict data,
                    global uint* restrict increments,
                    uint len) {
  const uint thread = get_local_id(0);
  const uint block = get_group_id(0);
  const uint threads_per_block = get_local_size(0);
  const uint elements_per_block = threads_per_block * 2;
  const uint global_offset = block * elements_per_block;
  // add the appropriate value to each block
  uint ai = 2 * thread;
  uint bi = 2 * thread + 1;
  uint ai_global = ai + global_offset;
  uint bi_global = bi + global_offset;
  uint increment = increments[block];
  if (ai_global < len) data[ai_global] += increment;
  if (bi_global < len) data[bi_global] += increment;
}
)__";

} // namespace <anonymous>

template <class T, class E = caf::detail::enable_if_t<is_integral<T>::value>>
T round_up(T numToRound, T multiple)  {
  return ((numToRound + multiple - 1) / multiple) * multiple;
}

int main() {
  actor_system_config cfg;
  cfg.load<opencl::manager>()
     .add_message_type<uvec>("uint_vector");
  actor_system system{cfg};
  cout << "Calculating exclusive scan of '" << problem_size
       << "' values." << endl;
  // ---- create data ----
  uvec values(problem_size);
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<uval> val_gen(0, 1023);
  std::generate(begin(values), end(values), [&]() { return val_gen(gen); });
  // ---- find device ----
  auto& mngr = system.opencl_manager();
  //
  string prefix = "GeForce";
  auto opt = mngr.find_device_if([&](const device_ptr dev) {
    auto& name = dev->name();
    return equal(begin(prefix), end(prefix), begin(name));
  });
  if (!opt) {
    cout << "No device starting with '" << prefix << "' found. "
         << "Will try the first OpenCL device available." << endl;
    opt = mngr.find_device();
  }
  if (!opt) {
    cerr << "Not OpenCL device available." << endl;
    return 0;
  } else {
    cerr << "Found device '" << (*opt)->name() << "'." << endl;
  }
  {
    // ---- general ----
    auto dev = move(*opt);
    auto prog = mngr.create_program(kernel_source, "", dev);
    scoped_actor self{system};
    // ---- config parameters ----
    auto half_block = dev->max_work_group_size() / 2;
    auto get_size = [half_block](size_t n) -> size_t {
      return round_up((n + 1) / 2, half_block);
    };
    auto nd_conf = [half_block, get_size](size_t dim) {
      return nd_range{dim_vec{get_size(dim)}, {}, dim_vec{half_block}};
    };
    auto reduced_ref = [&](const uref&, uval n) {
      // calculate number of groups from the group size from the values size
      return size_t{get_size(n) / half_block};
    };
    // default nd-range
    auto ndr = nd_range{dim_vec{half_block}, {}, dim_vec{half_block}};
    // ---- scan actors ----
    auto phase1 = mngr.spawn(
      prog, kernel_name_1, ndr,
      [nd_conf](nd_range& range, message& msg) -> optional<message> {
        return msg.apply([&](uvec& vec) {
          auto size = vec.size();
          range = nd_conf(size);
          return make_message(std::move(vec), static_cast<uval>(size));
        });
      },
      in_out<uval, val, mref>{},
      out<uval,mref>{reduced_ref},
      local<uval>{half_block * 2},
      priv<uval, val>{}
    );
    auto phase2 = mngr.spawn(
      prog, kernel_name_2, ndr,
      [nd_conf](nd_range& range, message& msg) -> optional<message> {
        return msg.apply([&](uref& data, uref& incs) {
          auto size = incs.size();
          range = nd_conf(size);
          return make_message(move(data), move(incs), static_cast<uval>(size));
        });
      },
      in_out<uval,mref,mref>{},
      in_out<uval,mref,mref>{},
      priv<uval, val>{}
    );
    auto phase3 = mngr.spawn(
      prog, kernel_name_3, ndr,
      [nd_conf](nd_range& range, message& msg) -> optional<message> {
        return msg.apply([&](uref& data, uref& incs) {
          auto size = incs.size();
          range = nd_conf(size);
          return make_message(move(data), move(incs), static_cast<uval>(size));
        });
      },
      in_out<uval,mref,val>{},
      in<uval,mref>{},
      priv<uval, val>{}
    );
    // ---- composed scan actor ----
    auto scanner = phase3 * phase2 * phase1;
    // ---- scan the data ----
    self->send(scanner, values);
    self->receive(
      [&](const uvec& results) {
        cout << "Received results." << endl;
        cout << " index | values |  scan  " << endl
             << "-------+--------+--------" << endl;
        for (size_t i = 0; i < problem_size; ++i)
          cout << setw(6) << i << " | " << setw(6) << values[i] << " | "
               << setw(6) << results[i] << endl;
      }
    );
  }
  system.await_all_actors_done();
  return 0;
}

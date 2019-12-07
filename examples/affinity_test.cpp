#include <iostream>

#include "caf/all.hpp"

#ifdef CAF_LINUX
#  include <sys/sysinfo.h>
#endif

#ifdef CAF_MACOS
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif // _GNU_SOURCE
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#endif // CAF_MAC

using std::endl;
using std::string;
using std::to_string;
using std::vector;

using namespace caf;

using print_core = atom_constant<atom("print_core")>;

// Get the affinity configuration of the current thread
string get_my_affinity() {
  string res = "  cores( ";
#if defined(CAF_LINUX)
  cpu_set_t mask;
  if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0) {
    auto nproc = get_nprocs();
    for (auto i = 0; i < nproc; i++) {
      if (CPU_ISSET(i, &mask)) {
        res += to_string(i) + " ";
      }
    }
  }
#elif defined(CAF_WINDOWS)
// TODO: implement getaffinity for Windows
#elif defined(CAF_MACOS)
  thread_affinity_policy_data_t affinity;
  mach_msg_type_number_t count = THREAD_AFFINITY_POLICY_COUNT;
  boolean_t getdefault = FALSE;
  if (thread_policy_get(pthread_mach_thread_np(pthread_self()), THREAD_AFFINITY_POLICY,
                        (thread_policy_t) &affinity,
                        &count, &getdefault)
      == 0) {
    res += to_string(affinity.affinity_tag) + " ";
  }
#elif defined(CAF_BSD)
  // TODO: implement getaffinity for FreeBSD
#endif
  res += ")";
  return res;
}

behavior event_actor(event_based_actor* self) {
  return {[=](print_core) {
    aout(self) << "[event_based] " << get_my_affinity() << endl;
  }};
}

behavior det_actor(event_based_actor* self) {
  return {[=](print_core) {
    aout(self) << "[ detached  ] " << get_my_affinity() << endl;
  }};
}

void block_actor(blocking_actor* self) {
  self->receive([=](print_core) {
    aout(self) << "[ blocking  ] " << get_my_affinity() << endl;
  });
}

int main() {
// Set affinity configuration
#ifndef CAF_MACOS
  // Windows, Linux and FreeBSD start counting cores from 0.
  actor_system_config cfg;
  // Pin the scheduled actors on core 0 and 1.
  cfg.set<vector<vector<int>>>("affinity.scheduled-actors", {{0, 1}});
  // Pin the first detached actor on core 0 and the second on core 1.
  cfg.set<vector<vector<int>>>("affinity.detached-actors", {{0}, {1}});
  // Pin the blocking actors on core 1.
  cfg.set<vector<vector<int>>>("affinity.blocking-actors", {{1}});
#else
  // Mac start counting cores from 1 and cannot set multiple cores per thread.
  actor_system_config cfg;
  // Pin the scheduled actors on core 2.
  cfg.set<vector<vector<int>>>("affinity.scheduled-actors", {{1}});
  // Pin the first detached actor on core 1 and the second on core 2.
  cfg.set<vector<vector<int>>>("affinity.detached-actors", {{1}, {2}});
  // Pin the blocking actors on core 1.
  cfg.set<vector<vector<int>>>("affinity.blocking-actors", {{1}});
#endif

  // Spawn all kind of actors and print the affinity configuration
  // of their undelining thread.
  actor_system system{cfg};
  anon_send(system.spawn(event_actor), print_core::value);
  anon_send(system.spawn(event_actor), print_core::value);
  anon_send(system.spawn<detached>(det_actor), print_core::value);
  anon_send(system.spawn<detached>(det_actor), print_core::value);
  anon_send(system.spawn(block_actor), print_core::value);
  anon_send(system.spawn(block_actor), print_core::value);
}

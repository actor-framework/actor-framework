#include <iostream>

#include "caf/all.hpp"

#ifdef CAF_LINUX
#  include <sys/sysinfo.h>
#endif // CAF_LINUX

using std::endl;
using std::string;
using std::to_string;

using namespace caf;

using print_core = atom_constant<atom("print_core")>;

// get the affinity configuration of the current thread
string get_my_affinity() {
  string res = "  cores( ";
#if defined(CAF_LINUX)
  cpu_set_t mask;
  if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
    perror("sched_getaffinity");
    return "";
  }
  auto nproc = get_nprocs();
  for (auto i = 0; i < nproc; i++) {
    if (CPU_ISSET(i, &mask)) {
      res += std::to_string(i) + " ";
    }
  }
#elif defined(CAF_WINDOWS)
  // TODO: implement getaffinity for windows
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

void caf_main(actor_system& system) {
  anon_send(system.spawn(event_actor), print_core::value);
  anon_send(system.spawn(event_actor), print_core::value);
  anon_send(system.spawn<detached>(det_actor), print_core::value);
  anon_send(system.spawn<detached>(det_actor), print_core::value);
  anon_send(system.spawn(block_actor), print_core::value);
  anon_send(system.spawn(block_actor), print_core::value);
}

CAF_MAIN()

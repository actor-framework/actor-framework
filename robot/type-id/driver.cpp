// This test driver reproduces the issue where a custom type ID block is defined
// but not initialized before creating the actor system. When the code tries to
// use a type from the uninitialized block, CAF should print a helpful error
// message via CAF_CRITICAL.

#include <caf/actor_system.hpp>
#include <caf/caf_main.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/type_id.hpp>

#ifdef CAF_WINDOWS
#  include <windows.h>

#  include <cstdlib>
#endif

struct my_custom_type {
  int value;
};

template <class Inspector>
bool inspect(Inspector& f, my_custom_type& x) {
  return f.object(x).fields(f.field("value", x.value));
}

CAF_BEGIN_TYPE_ID_BLOCK(my_module, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(my_module, (my_custom_type))

CAF_END_TYPE_ID_BLOCK(my_module)

int caf_main(caf::actor_system& sys) {
#ifdef CAF_WINDOWS
  // Suppress the Windows error dialog box.
  SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
  caf::scoped_actor self{sys};
  auto receiver = sys.spawn([](caf::event_based_actor* self) {
    return caf::behavior{
      [self](my_custom_type& x) {
        self->println("Received value: {}", x.value);
      },
    };
  });
  self->mail(my_custom_type{42}).send(receiver);
  self->wait_for(receiver);
  return EXIT_SUCCESS;
}

// Note: INTENTIONALLY not passing the type ID block to CAF_MAIN.
CAF_MAIN()

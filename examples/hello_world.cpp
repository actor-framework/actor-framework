#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::endl;
using std::string;

using namespace caf;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

behavior ping(event_based_actor* self, actor pong_actor, int num) {
  self->send(pong_actor, ping_atom::value, 0);
  return {
    [=](pong_atom, int x) {
      if (x == num)
        self->quit();
      return std::make_tuple(ping_atom::value, x + 1);
    }
  };
}

behavior pong() {
  return {
    [=](ping_atom, int x) {
      return std::make_tuple(pong_atom::value, x);
    }
  };
}


behavior vping() {
  return {
    [=](pong_atom, int) {
      // nop
    }
  };
}

behavior vpong() {
  return {
    [=](ping_atom, int) {
      // nop
    }
  };
}
struct config : actor_system_config {
  int n;
  int impl;
  config() : n(1000), impl(0) {
    opt_group{custom_options_, "global"}
    .add(n, "num,n", "set nr. of ping messages")
    .add(impl, "impl,i", "select implementation");
  }
};

// full CAF stack CAF with mailbox and scheduler
void impl0(actor_system& sys, const config& cfg) {
  sys.spawn(ping, sys.spawn(pong), cfg.n);
}

#define fixture                                                                \
  auto pi_hdl = actor_cast<strong_actor_ptr>(sys.spawn<lazy_init>(vping));     \
  auto po_hdl = actor_cast<strong_actor_ptr>(sys.spawn<lazy_init>(vpong));     \
  auto pi = dynamic_cast<scheduled_actor*>(pi_hdl->get());                     \
  auto po = dynamic_cast<scheduled_actor*>(po_hdl->get());                     \
  auto ctx = sys.dummy_execution_unit()

// bypass scheduler + mailbox, allocate mailbox elements, wrap message content
void impl1(actor_system& sys, const config& cfg) {
  fixture;
  for (auto i = 0; i < cfg.n; ++i) {
    {
      auto m1 = make_mailbox_element(pi_hdl, message_id::make(), {},
                                     make_message(ping_atom::value, i));
      po->activate(ctx, *m1);
    }
    {
      auto m2 = make_mailbox_element(pi_hdl, message_id::make(), {},
                                     make_message(pong_atom::value, i));
      pi->activate(ctx, *m2);
    }
  }
}

// bypass scheduler + mailbox, allocate mailbox elements, don't wrap message content
void impl2(actor_system& sys, const config& cfg) {
  fixture;
  for (auto i = 0; i < cfg.n; ++i) {
    {
      auto m1 = make_mailbox_element(pi_hdl, message_id::make(), {}, ping_atom::value, i);
      po->activate(ctx, *m1);
    }
    {
      auto m2 = make_mailbox_element(pi_hdl, message_id::make(), {}, pong_atom::value, i);
      pi->activate(ctx, *m2);
    }
  }
}

// bypass scheduler + mailbox, don't allocate mailbox elements, wrap message content
void impl3(actor_system& sys, const config& cfg) {
  fixture;
  mailbox_element_wrapper me{strong_actor_ptr{}, message_id::make(),
                             mailbox_element::forwarding_stack{},
                             make_message()};
  for (auto i = 0; i < cfg.n; ++i) {
    me.sender = pi_hdl;
    me.msg = make_message(ping_atom::value, i);
    po->activate(ctx, me);
    me.sender = po_hdl;
    me.msg = make_message(pong_atom::value, i);
    pi->activate(ctx, me);
  }
}

// bypass scheduler + mailbox, don't allocate mailbox elements, don't wrap message content
void impl4(actor_system& sys, const config& cfg) {
  fixture;
  using vals_t = mailbox_element_vals<atom_value, int>;
  struct x_t {
    x_t() {}
    ~x_t() {}
    union {  vals_t me; };
  } x;
  auto construct = [&](strong_actor_ptr hdl, atom_value atm, int i) {
    mailbox_element::forwarding_stack tmp;
    new (&x.me) vals_t(std::move(hdl), message_id::make(),
                       std::move(tmp), atm, i);
  };
  for (auto i = 0; i < cfg.n; ++i) {
    construct(pi_hdl, ping_atom::value, i);
    po->activate(ctx, x.me);
    x.me.~vals_t();
    construct(po_hdl, pong_atom::value, i);
    pi->activate(ctx, x.me);
    x.me.~vals_t();
  }
}

// bypass scheduler + mailbox, allocate mailbox elements, wrap message content
void impl5(actor_system& sys, const config& cfg) {
  fixture;
  for (auto i = 0; i < cfg.n; ++i) {
    {
      auto m1 = make_mailbox_element(pi_hdl, message_id::make(), {},
                                     make_message(ping_atom::value, i));
      po->mailbox().enqueue(m1.release());
      po->resume(ctx, 1);
    }
    {
      auto m2 = make_mailbox_element(pi_hdl, message_id::make(), {},
                                     make_message(pong_atom::value, i));
      pi->mailbox().enqueue(m2.release());
      pi->resume(ctx, 1);
    }
  }
}

// for i in {0..5}; do echo -n "impl:$i "; runtime ./hello_world --impl=$i --num=2000000; done
// for i in {0..5}; do echo -n "impl:$i "; runtime ./hello_world --caf#scheduler.policy='sharing' --caf#scheduler.max-threads=1 --impl=$i --num=2000000; done
void caf_main(actor_system& sys, const config& cfg) {
  if (cfg.impl == 0) {
    sys.spawn(ping, sys.spawn(pong), cfg.n);
    return;
  }
  using impl_fun = void (*)(actor_system&, const config&);
  impl_fun impls[] = {impl0, impl1, impl2, impl3, impl4, impl5};
  impls[cfg.impl](sys, cfg);
}

CAF_MAIN()

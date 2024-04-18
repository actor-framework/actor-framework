#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/actor_system.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/fwd.hpp"
#include "caf/spawn_options.hpp"

namespace caf::net {

/// Creates a new @ref actor_shell and registers it at the actor system.
template <class Handle> // Note: default is caf::actor; see fwd.hpp
actor_shell_ptr_t<Handle> make_actor_shell(socket_manager* mgr) {
  auto f = [](abstract_actor_shell* self, message& msg) -> result<message> {
    self->quit(make_error(sec::unexpected_message, std::move(msg)));
    return make_error(sec::unexpected_message);
  };

  using ptr_type = actor_shell_ptr_t<Handle>;
  using impl_type = typename ptr_type::element_type;
  auto& sys = mgr->mpx().system();
  actor_config cfg;
  cfg.mbox_factory = detail::actor_system_access(sys).mailbox_factory();
  auto hdl = sys.spawn_class<impl_type, no_spawn_options>(cfg, mgr);
  auto ptr = ptr_type{actor_cast<strong_actor_ptr>(std::move(hdl))};
  ptr->set_fallback(std::move(f));
  return ptr;
}

} // namespace caf::net

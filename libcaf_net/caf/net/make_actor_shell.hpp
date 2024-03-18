#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/typed_actor_shell.hpp"

#include "caf/actor_system.hpp"

namespace caf::net {

/// Creates a new @ref actor_shell and registers it at the actor system.
template <class Handle> // Note: default is caf::actor; see fwd.hpp
actor_shell_ptr_t<Handle> make_actor_shell(socket_manager* mgr) {
  auto f = [](abstract_actor_shell* self, message&) -> result<message> {
    auto err = error{sec::unexpected_message};
    self->quit(err);
    return err;
  };
  using ptr_type = actor_shell_ptr_t<Handle>;
  using impl_type = typename ptr_type::element_type;
  auto hdl = mgr->system().spawn<impl_type>(mgr);
  auto ptr = ptr_type{actor_cast<strong_actor_ptr>(std::move(hdl))};
  ptr->set_fallback(std::move(f));
  return ptr;
}

} // namespace caf::net

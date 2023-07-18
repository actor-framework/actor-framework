#include "caf/net/actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/typed_actor_shell.hpp"

#include "caf/actor_system.hpp"

namespace caf::net {

/// Creates a new @ref actor_shell and registers it at the actor system.
template <class Handle> // Note: default is caf::actor; see fwd.hpp
actor_shell_ptr_t<Handle>
make_actor_shell(actor_system& sys, async::execution_context_ptr loop) {
  auto f = [](abstract_actor_shell* self, message& msg) -> result<message> {
    self->quit(make_error(sec::unexpected_message, std::move(msg)));
    return make_error(sec::unexpected_message);
  };
  using ptr_type = actor_shell_ptr_t<Handle>;
  using impl_type = typename ptr_type::element_type;
  auto hdl = sys.spawn<impl_type>(loop);
  auto ptr = ptr_type{actor_cast<strong_actor_ptr>(std::move(hdl))};
  ptr->set_fallback(std::move(f));
  return ptr;
}

} // namespace caf::net

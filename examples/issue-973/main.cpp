#include "caf/io/all.hpp"

#include "caf/all.hpp"
#include "caf/mixin/actor_widget.hpp"

#include <iostream>

CAF_PUSH_WARNINGS
#include <QApplication>
#include <QMainWindow>
CAF_POP_WARNINGS

CAF_BEGIN_TYPE_ID_BLOCK(qtsupport, first_custom_type_id + 50)
  CAF_ADD_ATOM(qtsupport, set_name_atom)
CAF_END_TYPE_ID_BLOCK(qtsupport)

class MainWindow : public caf::mixin::actor_widget<QMainWindow> {
public:
  using super = caf::mixin::actor_widget<QMainWindow>;

  explicit MainWindow(QWidget* parent = nullptr) : super(parent) {
    std::cout << "MainWindow constructor" << std::endl;
  }

  void init(caf::actor_system& system) {
    // Initialize actor mix-in
    super::init(system);

    // Set message handler
    set_message_handler([=](caf::actor_companion*) -> caf::message_handler {
      std::cout << "MainWindow init" << std::endl;
      return {
        [=](caf::get_atom) { std::cout << "Hello world" << std::endl; },
        [=](set_name_atom) {
          std::cout << "Broken if caf_main isn't perfect" << std::endl;
        },
      };
    });
  }
};

// This code segfaults after enqueuing messages in ~message_data.
int caf_main(caf::actor_system& sys, const caf::actor_system_config& cfg) {
  auto [argc, argv] = cfg.c_args_remainder();
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  MainWindow mw;
  mw.init(sys);
  caf::scoped_actor self(sys);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     caf::get_atom_v);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     caf::timeout_atom_v);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     set_name_atom_v);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000), 50);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     std::string{"Test"});
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     caf::add_atom_v);
  self->delayed_send(mw.as_actor(), std::chrono::milliseconds(1000),
                     caf::actor{});
  return 0;
}

/*

// Even though very similar, this code doesn't cause a segfault

caf::behavior testee(caf::event_based_actor* self) {
  return {
    [=](caf::get_atom) { std::cout << "Hello world" << std::endl; },
    [=](set_name_atom) {
      std::cout << "Broken if caf_main isn't perfect" << std::endl;
    },
  };
}

int caf_main(caf::actor_system& sys, const caf::actor_system_config& cfg) {
  auto uut = sys.spawn(testee);
  caf::scoped_actor self(sys);
  self->delayed_send(uut, std::chrono::milliseconds(1000), caf::get_atom_v);
  self->delayed_send(uut, std::chrono::milliseconds(1000), caf::timeout_atom_v);
  self->delayed_send(uut, std::chrono::milliseconds(1000), set_name_atom_v);
  self->delayed_send(uut, std::chrono::milliseconds(1000), 50);
  self->delayed_send(uut, std::chrono::milliseconds(1000), std::string{"Test"});
  self->delayed_send(uut, std::chrono::milliseconds(1000), caf::add_atom_v);
  self->delayed_send(uut, std::chrono::milliseconds(1000), caf::actor{});
  return 0;
}
*/

// Both versions work similar.
CAF_MAIN(caf::id_block::qtsupport, caf::io::middleman)

// If I intentionally leave out the id_block::qtsupport, the first version
// of caf_main, featuring qt will segfault, while the second version with
// regular actors exits normally.
// CAF_MAIN(caf::io::middleman)

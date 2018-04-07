#pragma once

#include <exception>

#include "caf/all.hpp"
#include "caf/mixin/actor_widget.hpp"

CAF_PUSH_WARNINGS
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
CAF_POP_WARNINGS

class ChatWidget : public caf::mixin::actor_widget<QWidget> {
private:
  // -- Qt boilerplate code ----------------------------------------------------

  Q_OBJECT

public:
  // -- member types -----------------------------------------------------------

  using super = caf::mixin::actor_widget<QWidget>;

  using set_name_atom = caf::atom_constant<caf::atom("setName")>;

  using quit_atom = caf::atom_constant<caf::atom("quit")>;

  ChatWidget(QWidget* parent = nullptr, Qt::WindowFlags f = 0);

  ~ChatWidget();

  void init(caf::actor_system& system);

public slots:

  void sendChatMessage();
  void joinGroup();
  void changeName();

private:

  template<typename T>
  T* get(T*& member, const char* name) {
    if (member == nullptr) {
      member = findChild<T*>(name);
      if (member == nullptr)
        throw std::runtime_error("unable to find child: " + std::string(name));
    }
    return member;
  }

  inline QLineEdit* input() {
    return get(input_, "input");
  }

  inline QTextEdit* output() {
    return get(output_, "output");
  }

  inline void print(const QString& what) {
    output()->append(what);
  }

  caf::actor_system& system() {
    return self()->home_system();
  }

  QLineEdit* input_;
  QTextEdit* output_;
  std::string name_;
  caf::group chatroom_;
};

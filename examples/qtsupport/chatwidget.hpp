#pragma once

#include <exception>

#include "caf/all.hpp"
#include "caf/mixin/actor_widget.hpp"

CAF_PUSH_WARNINGS
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
CAF_POP_WARNINGS

CAF_BEGIN_TYPE_ID_BLOCK(qt_support, first_custom_type_id)

  CAF_ADD_ATOM(qt_support, set_name_atom)
  CAF_ADD_ATOM(qt_support, quit_atom)

CAF_END_TYPE_ID_BLOCK(qt_support)

class ChatWidget : public caf::mixin::actor_widget<QWidget> {
private:
  // -- Qt boilerplate code ----------------------------------------------------

  Q_OBJECT

public:
  // -- member types -----------------------------------------------------------

  using super = caf::mixin::actor_widget<QWidget>;

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

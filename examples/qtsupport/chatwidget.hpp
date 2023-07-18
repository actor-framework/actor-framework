#pragma once

#include "caf/net/lp/frame.hpp"

#include "caf/all.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/mixin/actor_widget.hpp"

#include <exception>

CAF_PUSH_WARNINGS
#include <QLineEdit>
#include <QTextEdit>
#include <QWidget>
CAF_POP_WARNINGS

CAF_BEGIN_TYPE_ID_BLOCK(qtsupport, first_custom_type_id)

  CAF_ADD_ATOM(qtsupport, quit_atom)

CAF_END_TYPE_ID_BLOCK(qtsupport)

class ChatWidget : public caf::mixin::actor_widget<QWidget> {
private:
  // -- Qt boilerplate code ----------------------------------------------------

  Q_OBJECT

public:
  // -- member types -----------------------------------------------------------

  using super = caf::mixin::actor_widget<QWidget>;

  using frame = caf::net::lp::frame;

  using publisher_type = caf::flow::multicaster<QString>;

  ChatWidget(QWidget* parent = nullptr);

  ~ChatWidget();

  void init(caf::actor_system& system, const std::string& name,
            caf::async::consumer_resource<frame> pull,
            caf::async::producer_resource<frame> push);

public slots:

  void sendChatMessage();
  void changeName();

private:
  template <typename T>
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
  QString name_;
  std::unique_ptr<publisher_type> publisher_;
};

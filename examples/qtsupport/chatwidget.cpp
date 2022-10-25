#include <string>
#include <string_view>
#include <utility>

#include "caf/all.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/scheduled_actor/flow.hpp"

CAF_PUSH_WARNINGS
#include <QInputDialog>
#include <QMessageBox>
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace std;
using namespace caf;

ChatWidget::ChatWidget(QWidget* parent)
  : super(parent), input_(nullptr), output_(nullptr) {
  // nop
}

ChatWidget::~ChatWidget() {
  // nop
}

void ChatWidget::init(actor_system& system, const std::string& name,
                      caf::async::consumer_resource<bin_frame> pull,
                      caf::async::producer_resource<bin_frame> push) {
  name_ = QString::fromUtf8(name);
  print("*** hello " + name_);
  super::init(system);
  self() //
    ->make_observable()
    .from_resource(pull)
    .do_finally([this] { //
      print("*** chatroom offline: lost connection to the server");
    })
    .for_each([this](const bin_frame& frame) {
      auto bytes = frame.bytes();
      auto str = std::string_view{reinterpret_cast<const char*>(bytes.data()),
                                  bytes.size()};
      if (std::all_of(str.begin(), str.end(), ::isprint)) {
        print(QString::fromUtf8(str.data(), str.size()));
      } else {
        QString msg = "<non-ascii-data of size ";
        msg += QString::number(bytes.size());
        msg += '>';
        print(msg);
      }
    });
  publisher_ = std::make_unique<publisher_type>(self());
  publisher_->as_observable()
    .map([](const QString& str) {
      auto encoded = str.toUtf8();
      auto bytes = caf::as_bytes(
        caf::make_span(encoded.data(), static_cast<size_t>(encoded.size())));
      return bin_frame{bytes};
    })
    .subscribe(push);
  set_message_handler([=](actor_companion*) -> message_handler {
    return {
      [=](quit_atom) { quit_and_close(); },
    };
  });
}

void ChatWidget::sendChatMessage() {
  auto cleanup = detail::make_scope_guard([=] { input()->setText(QString()); });
  QString line = input()->text();
  if (line.isEmpty()) {
    // Ignore empty lines.
  } else if (line.startsWith('/')) {
    vector<string> words;
    auto utf8 = QStringView{line}.mid(1).toUtf8();
    auto sv = std::string_view{utf8.constData(),
                               static_cast<size_t>(utf8.size())};
    split(words, sv, is_any_of(" "));
    if (words.size() == 2 && words[0] == "setName") {
      auto name = QString::fromUtf8(words[1]);
      if (!name.isEmpty())
        name_ = name;
    } else {
      print("*** list of commands:\n"
            "/setName <new name>\n");
      return;
    }
  } else {
    auto msg = name_;
    msg += ": ";
    msg += line;
    print("<you>: " + input()->text());
    if (publisher_) {
      publisher_->push(msg);
    }
  }
}

void ChatWidget::changeName() {
  auto name = QInputDialog::getText(this, "Change Name",
                                    "Please enter a new name");
  if (!name.isEmpty())
    name_ = name;
}

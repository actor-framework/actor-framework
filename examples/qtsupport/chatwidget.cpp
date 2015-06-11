#include <string>
#include <utility>

#include "caf/all.hpp"
#include "caf/detail/scope_guard.hpp"

CAF_PUSH_WARNINGS
#include <QMessageBox>
#include <QInputDialog>
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace std;
using namespace caf;

ChatWidget::ChatWidget(QWidget* parent, Qt::WindowFlags f)
: super(parent, f), input_(nullptr), output_(nullptr) {
        set_message_handler ([=](local_actor* self) -> message_handler {
        return {
            on(atom("join"), arg_match) >> [=](const group& what) {
                if (chatroom_) {
                    self->send(chatroom_, name_ + " has left the chatroom");
                    self->leave(chatroom_);
                }
                self->join(what);
                print(("*** joined " + to_string(what)).c_str());
                chatroom_ = what;
                self->send(what, name_ + " has entered the chatroom");
            },
            on(atom("setName"), arg_match) >> [=](string& name) {
                self->send(chatroom_, name_ + " is now known as " + name);
                name_ = std::move(name);
                print("*** changed name to "
                                  + QString::fromUtf8(name_.c_str()));
            },
            on(atom("quit")) >> [=] {
                close(); // close widget
            },
            [=](const string& txt) {
                // don't print own messages
                if (self != self->current_sender()) {
                    print(QString::fromUtf8(txt.c_str()));
                }
            },
            [=](const group_down_msg& gdm) {
                print("*** chatroom offline: "
                      + QString::fromUtf8(to_string(gdm.source).c_str()));
            }
        };
    });
}

void ChatWidget::sendChatMessage() {
    auto cleanup = detail::make_scope_guard([=] {
        input()->setText(QString());
    });
    QString line = input()->text();
    if (line.startsWith('/')) {
        vector<string> words;
        split(words, line.midRef(1).toUtf8().constData(), is_any_of(" "));
        message_builder(words.begin(), words.end()).apply({
            on("join", arg_match) >> [=](const string& mod, const string& g) {
                group gptr;
                try { gptr = group::get(mod, g); }
                catch (exception& e) {
                    print("*** exception: " + QString::fromUtf8((e.what())));
                }
                if (gptr) {
                    send_as(as_actor(), as_actor(), atom("join"), gptr);
                }
            },
            on("setName", arg_match) >> [=](const string& str) {
                send_as(as_actor(), as_actor(), atom("setName"), str);
            },
            others() >> [=] {
                print("*** list of commands:\n"
                      "/join <module> <group id>\n"
                      "/setName <new name>\n");
            }
        });
        return;
    }
    if (name_.empty()) {
        print("*** please set a name before sending messages");
        return;
    }
    if (! chatroom_) {
        print("*** no one is listening... please join a group");
        return;
    }
    string msg = name_;
    msg += ": ";
    msg += line.toUtf8().constData();
    print("<you>: " + input()->text());
    send_as(as_actor(), chatroom_, std::move(msg));
}

void ChatWidget::joinGroup() {
    if (name_.empty()) {
        QMessageBox::information(this,
                                 "No Name, No Chat",
                                 "Please set a name first.");
        return;
    }
    auto gname = QInputDialog::getText(this,
                                       "Join Group",
                                       "Please enter a group as <module>:<id>",
                                       QLineEdit::Normal,
                                       "remote:chatroom@localhost:4242");
    int pos = gname.indexOf(':');
    if (pos < 0) {
        QMessageBox::warning(this, "Not a Group", "Invalid format");
        return;
    }
    string mod = gname.left(pos).toUtf8().constData();
    string gid = gname.midRef(pos+1).toUtf8().constData();
    try {
        auto gptr = group::get(mod, gid);
        send_as(as_actor(), as_actor(), atom("join"), gptr);
    }
    catch (exception& e) {
        QMessageBox::critical(this, "Exception", e.what());
    }
}

void ChatWidget::changeName() {
    auto name = QInputDialog::getText(this, "Change Name", "Please enter a new name");
    if (! name.isEmpty()) {
        send_as(as_actor(), as_actor(), atom("setName"), name.toUtf8().constData());
    }
}

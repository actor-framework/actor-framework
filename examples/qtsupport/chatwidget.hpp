#include <exception>

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>

#include "caf/all.hpp"
#include "caf/mixin/actor_widget.hpp"

class ChatWidget : public caf::mixin::actor_widget<QWidget> {

    Q_OBJECT

    typedef caf::mixin::actor_widget<QWidget> super;

 public:

    ChatWidget(QWidget* parent = nullptr, Qt::WindowFlags f = 0);

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
                throw std::runtime_error("unable to find child: "
                                         + std::string(name));
        }
        return member;
    }

    inline QLineEdit* input() {
        return get(m_input, "input");
    }

    inline QTextEdit* output() {
        return get(m_output, "output");
    }

    inline void print(const QString& what) {
        output()->append(what);
    }

    QLineEdit* m_input;
    QTextEdit* m_output;
    std::string m_name;
    caf::group m_chatroom;

};

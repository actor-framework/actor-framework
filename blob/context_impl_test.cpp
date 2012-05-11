#include <iostream>
#include "cppa_fibre.h"

using std::cout;
using std::endl;

struct pseudo_worker {
    int m_value;

    pseudo_worker() : m_value(0) { }

    void operator()() {
        for (;;) {
            ++m_value;
            cout << "value = " << m_value << endl;
            cppa_fibre_yield(0);
        }
    }
};

void coroutine() {
    auto pw = reinterpret_cast<pseudo_worker*>(cppa_fibre_init_switch_arg()); (*pw)();
}

int main() {
    pseudo_worker pw;
    cppa_fibre fself;
    cppa_fibre fcoroutine;
    cppa_fibre_ctor(&fself);
    cppa_fibre_ctor2(&fcoroutine, coroutine, &pw);
    cppa_fibre_initialize(&fcoroutine);
    for (int i = 1; i < 11; ++i) {
        cout << "i = " << i << endl;
        cppa_fibre_switch(&fself, &fcoroutine);
    }
    cppa_fibre_dtor(&fself);
    cppa_fibre_dtor(&fcoroutine);
    return 0;
}

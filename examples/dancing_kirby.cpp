#include <iostream>
#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using namespace cppa;

// ASCII art figures
constexpr const char* figures[] = {
    "<(^.^<)",
    "<(^.^)>",
    "(>^.^)>"
};

// array of {figure, offset} pairs
constexpr size_t animation_steps[][2] = {
    {1,  7}, {0,  7}, {0,  6}, {0,  5}, {1,  5}, {2,  5}, {2,  6},
    {2,  7}, {2,  8}, {2,  9}, {2, 10}, {1, 10}, {0, 10}, {0,  9},
    {1,  9}, {2, 10}, {2, 11}, {2, 12}, {2, 13}, {1, 13}, {0, 13},
    {0, 12}, {0, 11}, {0, 10}, {0,  9}, {0,  8}, {0,  7}, {1,  7}
};

constexpr size_t animation_width = 20;

// "draws" an animation step: {offset_whitespaces}{figure}{padding}
void draw_kirby(size_t const (&animation)[2]) {
    cout.width(animation_width);
    cout << '\r';
    std::fill_n(std::ostream_iterator<char>{cout}, animation[1], ' ');
    cout << figures[animation[0]];
    cout.fill(' ');
    cout.flush();
}

void dancing_kirby() {
    // let's get it started
    send(self, atom("Step"));
    // iterate over animation_steps
    auto i = std::begin(animation_steps);
    receive_for(i, std::end(animation_steps)) (
        on<atom("Step")>() >> [&]() {
            draw_kirby(*i);
            // animate next step in 150ms
            delayed_send(self, std::chrono::milliseconds(150), atom("Step"));
        }
    );
}

int main() {
    cout << endl;
    dancing_kirby();
    cout << endl;
}

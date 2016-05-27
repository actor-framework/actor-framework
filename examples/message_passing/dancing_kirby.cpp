/******************************************************************************\
 * This example illustrates how to do time-triggered loops in libcaf.         *
\ ******************************************************************************/

#include <chrono>
#include <iostream>
#include <algorithm>
#include "caf/all.hpp"

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 56-75 (MessagePassing.tex)

using std::cout;
using std::endl;
using std::pair;

using namespace caf;

using step_atom = atom_constant<atom("step")>;

// ASCII art figures
constexpr const char* figures[] = {
  "<(^.^<)",
  "<(^.^)>",
  "(>^.^)>"
};

struct animation_step { size_t figure_idx; size_t offset; };

// array of {figure, offset} pairs
constexpr animation_step animation_steps[] = {
  {1,  7}, {0,  7}, {0,  6}, {0,  5}, {1,  5}, {2,  5}, {2,  6},
  {2,  7}, {2,  8}, {2,  9}, {2, 10}, {1, 10}, {0, 10}, {0,  9},
  {1,  9}, {2, 10}, {2, 11}, {2, 12}, {2, 13}, {1, 13}, {0, 13},
  {0, 12}, {0, 11}, {0, 10}, {0,  9}, {0,  8}, {0,  7}, {1,  7}
};

constexpr size_t animation_width = 20;

// "draws" an animation step by printing "{offset_whitespaces}{figure}{padding}"
void draw_kirby(const animation_step& animation) {
  cout.width(animation_width);
  // override last figure
  cout << '\r';
  // print offset
  std::fill_n(std::ostream_iterator<char>{cout}, animation.offset, ' ');
  // print figure
  cout << figures[animation.figure_idx];
  // print padding
  cout.fill(' ');
  // make sure figure is printed
  cout.flush();
}

// uses a message-based loop to iterate over all animation steps
void dancing_kirby(event_based_actor* self) {
  // let's get it started
  self->send(self, step_atom::value, size_t{0});
  self->become (
    [=](step_atom, size_t step) {
      if (step == sizeof(animation_step)) {
        // we've printed all animation steps (done)
        cout << endl;
        self->quit();
        return;
      }
      // print given step
      draw_kirby(animation_steps[step]);
      // animate next step in 150ms
      self->delayed_send(self, std::chrono::milliseconds(150),
                         step_atom::value, step + 1);
    }
  );
}

void caf_main(actor_system& system) {
  system.spawn(dancing_kirby);
}

CAF_MAIN()

// This example illustrates how to do time-triggered loops in CAF.

#include <algorithm>
#include <chrono>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::pair;

using namespace caf;

// ASCII art figures.
constexpr const char* figures[] = {
  "<(^.^<)",
  "<(^.^)>",
  "(>^.^)>",
};

// Bundes an index to an ASCII art figure plus its position on screen.
struct animation_step {
  size_t figure_idx;
  size_t offset;
};

// Array of {figure, offset} pairs.
constexpr animation_step animation_steps[] = {
  {1, 7},  {0, 7},  {0, 6},  {0, 5},  {1, 5},  {2, 5},  {2, 6},
  {2, 7},  {2, 8},  {2, 9},  {2, 10}, {1, 10}, {0, 10}, {0, 9},
  {1, 9},  {2, 10}, {2, 11}, {2, 12}, {2, 13}, {1, 13}, {0, 13},
  {0, 12}, {0, 11}, {0, 10}, {0, 9},  {0, 8},  {0, 7},  {1, 7},
};

// Width of the printed area.
constexpr size_t animation_width = 20;

// Draws an animation step by printing "{offset_whitespaces}{figure}{padding}".
void draw_kirby(const animation_step& animation) {
  cout.width(animation_width);
  // Override last figure.
  cout << '\r';
  // Print left padding (offset).
  std::fill_n(std::ostream_iterator<char>{cout}, animation.offset, ' ');
  // Print figure.
  cout << figures[animation.figure_idx];
  // Print right padding.
  cout.fill(' ');
  // Make sure figure is visible.
  cout.flush();
}

// --(rst-dancing-kirby-begin)--
// Uses a message-based loop to iterate over all animation steps.
behavior dancing_kirby(event_based_actor* self) {
  // Let's get started.
  auto i = std::begin(animation_steps);
  auto e = std::end(animation_steps);
  self->send(self, update_atom_v);
  return {
    [=](update_atom) mutable {
      // We're done when reaching the past-the-end position.
      if (i == e) {
        cout << endl;
        self->quit();
        return;
      }
      // Print current animation step.
      draw_kirby(*i);
      // Animate next step in 150ms.
      ++i;
      self->delayed_send(self, std::chrono::milliseconds(150), update_atom_v);
    },
  };
}
// --(rst-dancing-kirby-end)--

void caf_main(actor_system& system) {
  system.spawn(dancing_kirby);
}

CAF_MAIN()

/******************************************************************************
 * This example shows how to implement serialize/deserialize to announce      *
 * non-trivial data structures to the libcaf type system.                     *
 *                                                                            *
 * Announce() auto-detects STL compliant containers and provides              *
 * an easy way to tell libcaf how to serialize user defined types.            *
 * See announce_example 1-4 for usage examples.                               *
 *                                                                            *
 * You should use "hand written" serialize/deserialize implementations        *
 * if and only if there is no other way.                                      *
 ******************************************************************************/

#include <cstdint>
#include <iostream>
#include "caf/all.hpp"
#include "caf/to_string.hpp"

using std::cout;
using std::endl;

using namespace caf;

namespace {

// a node containing an integer and a vector of children
struct tree_node {
  std::uint32_t value;
  std::vector<tree_node> children;

  explicit tree_node(std::uint32_t v = 0) : value(v) {
    // nop
  }

  tree_node& add_child(std::uint32_t v = 0) {
    children.emplace_back(v);
    return *this;
  }

  tree_node(const tree_node&) = default;

  // recursively print this node and all of its children to stdout
  void print() const {
    // format is: value { children0, children1, ..., childrenN }
    // e.g., 10 { 20 { 21, 22 }, 30 }
    cout << value;
    if (children.empty() == false) {
      cout << " { ";
      auto begin = children.begin();
      auto end = children.end();
      for (auto i = begin; i != end; ++i) {
        if (i != begin) {
          cout << ", ";
        }
        i->print();
      }
      cout << " } ";
    }
  }

};

// a very primitive tree implementation
struct tree {
  tree_node root;

  // print tree to stdout
  void print() const {
    cout << "tree::print: ";
    root.print();
    cout << endl;
  }

};

// tree nodes are equals if values and all values of all children are equal
bool operator==(const tree_node& lhs, const tree_node& rhs) {
  return (lhs.value == rhs.value) && (lhs.children == rhs.children);
}

bool operator==(const tree& lhs, const tree& rhs) {
  return lhs.root == rhs.root;
}

// abstract_uniform_type_info implements all functions of uniform_type_info
// except for serialize() and deserialize() if the template parameter T:
// - does have a default constructor
// - does have a copy constructor
// - does provide operator==
class tree_type_info : public abstract_uniform_type_info<tree> {
public:
  tree_type_info() : abstract_uniform_type_info<tree>("tree") {
    // nop
  }

protected:
  void serialize(const void* ptr, serializer* sink) const {
    // ptr is guaranteed to be a pointer of type tree
    auto tree_ptr = reinterpret_cast<const tree*>(ptr);
    // recursively serialize nodes, beginning with root
    serialize_node(tree_ptr->root, sink);
  }

  void deserialize(void* ptr, deserializer* source) const {
    // ptr is guaranteed to be a pointer of type tree
    auto tree_ptr = reinterpret_cast<tree*>(ptr);
    tree_ptr->root.children.clear();
    // recursively deserialize nodes, beginning with root
    deserialize_node(tree_ptr->root, source);
  }

private:
  void serialize_node(const tree_node& node, serializer* sink) const {
    // value, ... children ...
    sink->write_value(node.value);
    sink->begin_sequence(node.children.size());
    for (const tree_node& subnode : node.children) {
      serialize_node(subnode, sink);
    }
    sink->end_sequence();
  }

  void deserialize_node(tree_node& node, deserializer* source) const {
    // value, ... children ...
    auto value = source->read<std::uint32_t>();
    node.value = value;
    auto num_children = source->begin_sequence();
    for (size_t i = 0; i < num_children; ++i) {
      node.add_child();
      deserialize_node(node.children.back(), source);
    }
    source->end_sequence();
  }
};

using tree_vector = std::vector<tree>;

// receives `remaining` messages
void testee(event_based_actor* self, size_t remaining) {
  auto set_next_behavior = [=] {
    if (remaining > 1) testee(self, remaining - 1);
    else self->quit();
  };
  self->become (
    [=](const tree& tmsg) {
      // prints the tree in its serialized format:
      // @<> ( { tree ( 0, { 10, { 11, { }, 12, { }, 13, { } }, 20, { 21, { }, 22, { } } } ) } )
      cout << "to_string(self->current_message()): "
         << to_string(self->current_message())
         << endl;
      // prints the tree using the print member function:
      // 0 { 10 { 11, 12, 13 } , 20 { 21, 22 } }
      tmsg.print();
      set_next_behavior();
    },
    [=](const tree_vector& trees) {
      // prints "received 2 trees"
      cout << "received " << trees.size() << " trees" << endl;
      // prints:
      // @<> ( {
      //   std::vector<tree, std::allocator<tree>> ( {
      //   tree ( 0, { 10, { 11, { }, 12, { }, 13, { } }, 20, { 21, { }, 22, { } } } ),
      //   tree ( 0, { 10, { 11, { }, 12, { }, 13, { } }, 20, { 21, { }, 22, { } } } )
      //   )
      // } )
      cout << "to_string: " << to_string(self->current_message()) << endl;
      set_next_behavior();
    }
  );
}

} // namespace <anonymous>

int main() {
  // the tree_type_info is owned by libcaf after this function call
  announce(typeid(tree), std::unique_ptr<uniform_type_info>{new tree_type_info});
  announce<tree_vector>("tree_vector");

  tree t0; // create a tree and fill it with some data

  t0.root.add_child(10);
  t0.root.children.back().add_child(11).add_child(12).add_child(13);

  t0.root.add_child(20);
  t0.root.children.back().add_child(21).add_child(22);

  /*
    tree t is now:
         0
        / \
       /   \
      /   \
      10     20
     / |\   /  \
    /  | \   /  \
     11 12 13 21  22
  */

  { // lifetime scope of self
    scoped_actor self;

    // spawn a testee that receives two messages
    auto t = spawn(testee, size_t{2});

    // send a tree
    self->send(t, t0);

    // send a vector of trees
    tree_vector tvec;
    tvec.push_back(t0);
    tvec.push_back(t0);
    self->send(t, tvec);
  }

  await_all_actors_done();
  shutdown();
  return 0;
}

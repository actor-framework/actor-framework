This document specifies the coding style for the C++ Actor Framework.
The style is based on the
[Google C++ Style Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml).

# Example for the Impatient

#### `libcaf_example/caf/example/my_class.hpp`

```cpp
#ifndef CAF_EXAMPLE_MY_CLASS_HPP
#define CAF_EXAMPLE_MY_CLASS_HPP

#include <string>

namespace caf {
namespace example {

class my_class {
 public:
  /**
   * @brief Constructs `my_class`.
   *
   * The class is only being used as style guide example.
   */
  my_class();
  
  /**
   * @brief Destructs `my_class`.
   */
  ~my_class();

  /**
   * @brief Gets the name of this instance.
   *        Some more brief description.
   *
   * Long description goes here.
   */
  inline const std::string& name() const {
    return m_name;
  }
  
  /**
   * @brief Sets the name of this instance.
   */
  inline void name(std::string new_name) {
    m_name = std::move(new_name);
  }
  
  /**
   * @brief Prints the name to `STDIN`.
   */
  void print_name() const;

  /**
   * @brief Does something. Maybe.
   */
  void do_something();

 private:
  std::string m_name;
};

} // namespace example
} // namespace caf

#endif // CAF_EXAMPLE_MY_CLASS_HPP
```

#### `libcaf_example/src/my_class.cpp`

```cpp
#include "caf/example/my_class.hpp"

#include <iostream>

namespace caf {
namespace example {

namespace {

constexpr const char default_name[] = "my object";

} // namespace <anonymous>

my_class::my_class() : m_name(default_name) {
  // nop
}

my_class::~my_class() {
  // nop
}

void my_class::print_name() const {
  std::cout << name() << std::endl;
}

void my_class::do_something() {
  if (name() == default_name) {
    std::cout << "You didn't gave me a proper name, so I "
              << "refuse to do something."
              << std::endl;
  } else {
    std::cout << "You gave me the name " << name() 
              << "... Do you really think I'm willing to do something "
                 "for you after insulting me like that?"
              << std::endl;
  }
}

} // namespace example
} // namespace caf

```

# General rules

- Use 2 spaces per indentation level.
- The maximum number of characters per line is 80.
- Never use tabs.
- Vertical whitespaces separate functions and are not used inside functions:
  use comments to document logical blocks.
- Header filenames end in `.hpp`, implementation files end in `.cpp`.
- Member variables use the prefix `m_`.
- Thread-local variables use the prefix `t_`.
- Never declare more than one variable per line.
- `*` and `&` bind to the *type*.
- Class names, constants, and function names are all-lowercase with underscores.
- Template parameter names use CamelCase.
- Namespaces do not increase the indentation level.
- Access modifiers, e.g. `public`, are indented one space.
- Use the order `public`, `protected`, and then `private`. 
- Always use `auto` to declare a variable unless you cannot initialize it
  immediately or if you actually want a type conversion. In the latter case,
  provide a comment why this conversion is necessary.
- Never use unwrapped, manual resource management such as `new` and `delete`.
- Do not use `typedef`. The syntax provided by `using` is much cleaner.
- Use `typename` only when referring to dependent names.
- Keywords are always followed by a whitespace: `if (...)`, `template <...>`,
  `while (...)`, etc.
- Always use `{}` for bodies of control structures such as `if` or `while`,
  even for bodies consiting only of a single statement.
- Opening braces belong to the same line:
  ```cpp

  if (my_condition) {
    my_fun();
  } else {
    my_other_fun();
  }
  ```
- Use standard order for readability: C standard libraries, C++ standard
  libraries, other libraries, (your) CAF headers:
  ```cpp
  
  #include <sys/types.h>

  #include <vector>
  
  #include "some/other/library.hpp"
  
  #include "caf/example/myclass.hpp"
  ```

# Headers

- Each `.cpp` file has an associated `.hpp` file.
  Exceptions to this rule are unit tests and `main.cpp` files.
- Each class has its own pair of header and implementation
  files and the relative path is derived from its full name.
  For example, the header file for `caf::example::my_class` of `libcaf_example`
  is located at `libcaf_example/caf/example/my_class.hpp`.
  Source files simply belong to the `src` folder of the component, e.g.,
  `libcaf_example/src/my_class.cpp`.
- All header files should use `#define` guards to prevent multiple inclusion. 
  The symbol name is `<RELATIVE>_<PATH>_<TO>_<FILE>_HPP`.
- Do not `#include` when a forward declaration suffices.
- Each library component must provide a `fwd.hpp` header providing forward
  declartations for all types used in the user API.
- Each library component must provide an `all.hpp` header including
  all headers for the user API and the main page for the documentation.
- Use `inline` for small functions (rule of thumb: 10 lines or less).

## Template Metaprogramming

Despite its power, template metaprogramming came to the language pretty
much by accident. Templates were never meant to be used for compile-time
algorithms and type transformations. This is why C++ punishes metaprogramming
with an insane amount of syntax noise. In CAF, we make excessive use of
templates. To keep the code readable despite all the syntax noise, we have some
extra rules for formatting metaprogramming code.

- Brake `using name = ...` statements always directly after `=` if it
  does not fit in one line.
- Consider the *semantics* of a metaprogramming function. For example,
  `std::conditional` is an if-then-else construct. Hence, place the if-clause
  on its own line and do the same for the two cases.
- Use one level of indentation per "open" template and place the closing `>`,
  `>::type` or `>::value` on its own line. For example:
  ```cpp
  
  using optional_result_type = 
    typename std::conditional<
      std::is_same<result_type, void>::value,
      bool,
      optional<result_type>
    >::type;
  // think of it as the following (not valid C++):
  auto optional_result_type = 
    conditional {
      if   (result_type == void)
      then (bool)
      else (optional<result_type>)
    };
  ```
- Note that this is not necessary when simply defining a type alias.
  When dealing with "ordinary" templates, indenting based on the position of
  the opening `<` is ok too, e.g.:
  ```cpp

  using response_handle_type = response_handle<Subtype, message,
                                               ResponseHandleTag>;
  ```

# Miscellaneous

## Rvalue references

- Use rvalue references whenever it makes sense.
- Write move constructors and assignment operators if possible.

## Casting

- Never use C-style casts.

## Preprocessor Macros

- Use macros if and only if you can't get the same result by using inline
  functions or proper constants.
- Macro names use the form `CAF_<COMPONENT>_<NAME>`.

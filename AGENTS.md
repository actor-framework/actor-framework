# CAF: The C++ Actor Framework

CAF is an open source framework that offers a programming environment based on
the Actor Model of computation. CAF features lightweight & fast actor
implementations, data flows, HTTP and WebSocket support, pattern matching for
messages, metrics, distributed actors, and more.

# How to build this project

When asked to build the project, use:
- `cmake --build build`

# How to run tests

When asked to run tests, use:
- `ctest --test-dir build` to run everything
- `ctest --test-dir build -R <name>` to run specific tests

# Coding conventions

The coding style for this project is outlined in the file `CONTRIBUTING.md`.

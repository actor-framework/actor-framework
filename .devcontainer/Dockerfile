FROM debian:12

# Base setup
RUN apt-get update -y \
    && apt-get install -y --no-install-recommends \
      ca-certificates \
      clang-format-19 \
      clangd \
      cmake \
      cppcheck \
      curl \
      g++ \
      git \
      jq \
      libssl-dev \
      make \
      ninja-build \
      python3 \
      python3-pip \
      vim

# Set clang-format-19 as the default clang-format.
RUN update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-19 100 \
    && update-alternatives --set clang-format /usr/bin/clang-format-19

# Install Robot dependencies
COPY robot/dependencies.txt /tmp/robot-dependencies.txt
RUN python3 -m pip install -r /tmp/robot-dependencies.txt --break-system-packages

# Have CMake use Ninja as the default generator
ENV CMAKE_GENERATOR=Ninja

# Set a sane default for CTest to show output on failure
ENV CTEST_OUTPUT_ON_FAILURE=ON

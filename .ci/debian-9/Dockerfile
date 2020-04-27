FROM debian:stretch

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  clang-7 \
  cmake \
  git \
  libssl-dev \
  make \
 && apt autoclean

ENV CXX=/usr/bin/clang++-7

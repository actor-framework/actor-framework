FROM ubuntu:16.04

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  clang-8 \
  cmake \
  git \
  libssl-dev \
  make \
 && apt autoclean

ENV CXX=/usr/bin/clang++-8

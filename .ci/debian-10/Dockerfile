FROM debian:buster

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  cmake \
  g++-8 \
  git \
  libssl-dev \
  make \
 && apt autoclean

ENV CXX=/usr/bin/g++-8

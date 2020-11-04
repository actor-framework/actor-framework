FROM debian:10

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  cmake \
  g++ \
  git \
  libssl-dev \
  make \
 && apt autoclean

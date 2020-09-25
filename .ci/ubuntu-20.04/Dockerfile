FROM ubuntu:20.04

ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  cmake \
  g++ \
  git \
  libssl-dev \
  make \
 && apt-get autoclean
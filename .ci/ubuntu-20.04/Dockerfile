FROM ubuntu:20.04

ADD ./robot/dependencies.txt .

RUN apt update -y \
    && apt upgrade -y \
    && DEBIAN_FRONTEND="noninteractive" apt install -y \
         cmake \
         g++ \
         git \
         libssl-dev \
         make \
    && apt-get autoclean

RUN python3 -m pip install -r dependencies.txt


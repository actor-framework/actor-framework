FROM debian:11

ADD ./robot/dependencies.txt .

RUN apt update -y \
    && apt upgrade -y \
    && apt install -y \
         g++ \
         cmake \
         git \
         libssl-dev \
         make \
    && apt autoclean

RUN python3 -m pip install -r dependencies.txt

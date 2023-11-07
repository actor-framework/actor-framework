FROM almalinux:9

ADD ./robot/dependencies.txt .

RUN dnf update -y \
    && dnf install -y \
         cmake \
         gcc-c++ \
         git \
         libasan \
         libubsan \
         make \
         openssl-devel \
         python39 \
         python39-pip \
    && dnf clean all

RUN python3 -m pip install -r dependencies.txt

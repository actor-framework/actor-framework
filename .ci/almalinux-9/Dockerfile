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
    && dnf clean all

RUN python -m pip install -r dependencies.txt

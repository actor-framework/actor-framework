FROM fedora:36

RUN dnf update -y \
    && dnf clean all \
    && dnf install -y \
         cmake \
         gcc-c++ \
         git \
         libasan \
         libubsan \
         make \
         openssl-devel \
         python3 \
         python3-pip \
    && dnf clean all

RUN python3 -m pip install \
      robotframework \
      robotframework-requests \
      robotframework-websocketclient

FROM fedora:32

RUN dnf update -y && \
  dnf clean all

RUN dnf install -y \
  git \
  make \
  cmake \
  gcc-c++ \
  openssl-devel \
  libasan \
  libubsan \
  libcurl-devel \
  qt5-devel \
  protobuf-devel \
 && dnf clean all

FROM centos:6

RUN yum update -y \
   && yum install -y centos-release-scl \
   && yum install -y epel-release \
   && yum update -y \
   && yum clean all

RUN yum install -y \
  cmake3 \
  devtoolset-7 \
  devtoolset-7-libasan-devel \
  devtoolset-7-libubsan-devel \
  git \
  make \
  openssl-devel \
 && yum clean all

ENV CXX=/opt/rh/devtoolset-7/root/usr/bin/g++

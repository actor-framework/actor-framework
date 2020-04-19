############################################################
# Dockerfile for Jenkins slave with gcc 7.3
# Based on CentOS 6
############################################################

FROM centos:6
MAINTAINER Jakob Otto

# Install Essentials
RUN yum update -y \
   && yum install -y centos-release-scl \
   && yum install -y epel-release \
   && yum update -y \
   && yum clean all

##################### INSTALL TOOLS ########################

# Install Packages
RUN yum install -y git \
  wget \
  make \
  sudo \
  java-1.8.0-openjdk \
  cmake3 \
  openssl-devel \
  libubsan \
  python-pip \
  devtoolset-7 \
  devtoolset-7-libasan-devel \
 && yum clean all

####################### SETUP SSH #########################

# Create Jenkins User
RUN useradd jenkins -m -s /bin/bash

# Setup home for Jenkins
RUN mkdir /home/jenkins/bin
RUN chown -R jenkins /home/jenkins
RUN chgrp -R jenkins /home/jenkins

# Create symlinks to cmake3 binaries in local bin directory.
RUN ln -s /usr/bin/cmake3 /home/jenkins/bin/cmake
RUN ln -s /usr/bin/ctest3 /home/jenkins/bin/ctest

# Expose SSH port and run SSHD
EXPOSE 22
CMD ["/usr/sbin/sshd","-D"]

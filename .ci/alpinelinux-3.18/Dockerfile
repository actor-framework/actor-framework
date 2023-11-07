FROM alpine:3.18

ADD . /foo
RUN ls -la /foo

RUN apk add --no-cache \
         cmake\
         g++ \
         gcc\
         git \
         linux-headers \
         make \
         musl-dev \
         openssl-dev \
         python3 \
         py3-pip

RUN ls -la /foo

RUN python3 -m pip install -r sources/robot/dependencies.txt

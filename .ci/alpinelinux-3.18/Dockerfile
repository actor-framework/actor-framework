FROM alpine:3.18

ADD ./robot/dependencies.txt /bar
RUN ls -la /bar
ADD ./robot/dependencies.txt .
RUN ls -la .

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

RUN python3 -m pip install -r /bar/robot/dependencies.txt

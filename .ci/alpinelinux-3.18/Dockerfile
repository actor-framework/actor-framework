FROM alpine:3.18

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

RUN python3 -m pip install \
      robotframework \
      robotframework-requests \
      robotframework-websocketclient

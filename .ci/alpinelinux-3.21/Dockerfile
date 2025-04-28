FROM alpine:3.21

COPY ./robot/dependencies.txt .

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

RUN python3 -m pip install -r dependencies.txt --break-system-packages

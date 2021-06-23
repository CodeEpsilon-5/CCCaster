FROM ubuntu:21.04 as base

RUN apt-get update && apt-get install gcc-mingw-w64 make git rsync g++-mingw-w64-i686 mingw-w64-i686-dev zip -y
RUN mkdir /cccaster

WORKDIR /cccaster

FROM base as dev

ADD . /cccaster


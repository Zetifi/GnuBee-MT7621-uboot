FROM 32bit/ubuntu:16.04

RUN apt-get update -qq
RUN apt-get install -y \
        build-essential \
        libncurses-dev \
        libz-dev \
        bc \
        && apt-get -y autoremove \
        && apt-get clean \
        && rm -rf /var/lib/apt/lists/*

ADD ./toolchain/mips-2012.03.tar.bz2 /opt

RUN mkdir -p /home/build
WORKDIR /home/build

ENV PATH "/opt/mips-2012.03/bin:$PATH"

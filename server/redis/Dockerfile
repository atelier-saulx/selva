FROM ubuntu

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libssl-dev git

RUN git clone --depth 1 --branch 6.0.10 https://github.com/redis/redis
COPY . /redis-patches
RUN cd redis && \
    git apply /redis-patches/0001-Add-RM_ReplicateVerbatimArgs.patch && \
    git apply /redis-patches/0002-Add-ReplyWithBinaryBuffer-for-sending-bin-bufs.patch && \
    git apply /redis-patches/0003-Add-RedisModule_StopTimerUnsafe.patch && \
    make PROG_SUFFIX="-selva"

CMD cp /redis/src/redis-server-selva /dist/redis-server-selva; echo "Done"

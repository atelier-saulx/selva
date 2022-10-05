FROM ubuntu:20.04

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libssl-dev git

#RUN git clone --branch v1.0.1 https://github.com/redis/hiredis
#RUN cd hiredis && make && make install

WORKDIR /data
COPY ./jemalloc /data/jemalloc
COPY ./selva /data/selva
COPY ./locale /data/locale
COPY ./binaries/linux_x64 /data/binaries/linux_x64
ENV LOCPATH="/data/locale.build"

CMD \
    cd /data/jemalloc && make -j4; cp /data/binaries/linux_x64/libjemalloc_selva* /dist/; \
    cd /data/selva && make -j4 && cp ./module.so /dist/selva.so; \
    cd /data/locale && make -j4; \
    cd "$LOCPATH" && find "./" -type f -exec install -D "--group=$DOCKER_GROUP" "--owner=$DOCKER_USER" "{}" "/locale/{}" \;

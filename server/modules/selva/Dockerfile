FROM ubuntu

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libssl-dev git

RUN git clone https://github.com/redis/hiredis
RUN cd hiredis && make && make install

# RUN git clone --depth 1 https://github.com/atelier-saulx/selva

# TODO: move redis  stuff to its own Dockerfile in redis folder
# RUN git clone --depth 1 --branch 6.0.10 https://github.com/redis/redis
# COPY ../../redis /redis-patches
# RUN cd redis && git apply /redis-patches/0001-Add-RM_ReplicateVerbatimArgs.patch && make PROG_SUFFIX="-selva"
# cp /redis/src/redis-server-selva /dist/redis-server-selva

WORKDIR /data
COPY ./ ./

CMD make clean;  make; cp ./module.so /dist/selva.so 

FROM ubuntu

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libssl-dev git

RUN git clone https://github.com/redis/hiredis
RUN cd hiredis && make && make install

RUN git clone --depth 1 --branch fastmode-sm https://github.com/atelier-saulx/selva
RUN git clone --depth 1 --branch 6.0.10 https://github.com/redis/redis
RUN cd redis && git apply ../selva/server/redis/0001-Add-RM_ReplicateVerbatimArgs.patch && make PROG_SUFFIX="-selva"


WORKDIR /data

COPY ./ ./

CMD make clean;  make; cp ./module.so /dist/selva.so; cp /redis/src/redis-server-selva /dist/redis-server-selva

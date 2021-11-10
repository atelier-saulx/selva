FROM ubuntu

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libssl-dev git

RUN git clone --branch v1.0.1 https://github.com/redis/hiredis
RUN cd hiredis && make && make install

WORKDIR /data
COPY ./ ./

CMD make clean;  make; cp ./module.so /dist/selva.so 

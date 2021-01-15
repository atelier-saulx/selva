FROM redis

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev libhiredis-dev libssl-dev

COPY ./ ./

WORKDIR /data

CMD make clean;  make; cp ./module.so /dist/selva.so

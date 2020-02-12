FROM redis

RUN apt-get update -y
RUN apt-get install -y build-essential uuid-dev

COPY ./ ./

WORKDIR /data

CMD make clean;  make; cp ./module.so /dist/selva.so
FROM 5keeve/pocketbook-sdk:6.3.0-b288-v1 AS builder

COPY . /project/

RUN cd /project \
 && mkdir -p build \
 && cd build \
 && cmake .. \
 && cmake --build . \
;

FROM scratch AS exporter

COPY --from=builder /project/build/SGTPuzzles.app ./

FROM alpine:latest AS build

WORKDIR /app

RUN mkdir bin

RUN apk add make gcc musl-dev

COPY Makefile .
COPY src ./src

RUN make

FROM alpine:latest AS runtime

WORKDIR /app

COPY --from=build /app/bin/proxy .

RUN ls

ENTRYPOINT [ "./proxy" ]

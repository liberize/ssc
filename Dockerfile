FROM alpine:latest
WORKDIR /ssc
COPY . .
RUN apk add --no-cache g++ perl binutils libarchive-dev acl-dev zlib-dev libarchive-static acl-static zlib-static
ENTRYPOINT ["/ssc/ssc"]

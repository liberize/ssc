FROM alpine:latest
RUN apk add --no-cache g++ perl binutils libarchive-dev acl-dev zlib-dev libarchive-static acl-static zlib-static
COPY . /ssc
WORKDIR /workspace
ENTRYPOINT ["/ssc/ssc"]

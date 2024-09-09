FROM alpine:latest
RUN apk add --no-cache git g++ perl binutils libarchive-dev acl-dev zlib-dev libarchive-static acl-static zlib-static
RUN git clone https://github.com/liberize/ssc
WORKDIR /ssc
CMD /bin/sh

FROM alpine:3.17.1
RUN apk update && apk add bash curl
ENV PATH="$PATH:/bin"
COPY get_urbit_code.sh reset_urbit_code.sh start_urbit.sh urbit /bin/
EXPOSE 80/tcp
EXPOSE 34343/udp
VOLUME /urbit
WORKDIR /urbit
CMD start-urbit

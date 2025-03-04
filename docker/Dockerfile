FROM alpine:3.17.1
RUN apk update && apk add bash curl
ENV PATH="$PATH:/bin"
COPY get_urbit_code.sh /bin/get_urbit_code
COPY reset_urbit_code.sh /bin/reset_urbit_code
COPY start_urbit.sh /bin/start_urbit
COPY urbit /bin/
EXPOSE 80/tcp
EXPOSE 34343/udp
VOLUME /urbit
WORKDIR /urbit
ENTRYPOINT ["/bin/start_urbit"]

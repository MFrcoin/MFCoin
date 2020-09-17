FROM debian AS builder

ARG VERSION=latest
ARG WALLET=true
ARG UPNPC=true
ARG USE_OLD_BERKLEYDB=true

COPY --from=kamehb/berkleydb4.8-dev /opt/db /opt/db

RUN apt update && \
    apt install -y build-essential libc6-dev binutils git autoconf pkg-config automake libtool libdb-dev libdb++-dev libboost-all-dev libssl-dev libminiupnpc-dev libevent-dev dpkg-dev

COPY . /MFCoin

WORKDIR /MFCoin

RUN if ! [ "$VERSION" = latest ]; then \
        git checkout "tags/v.$VERSION"; \
    fi

RUN git submodule update --init --recursive

RUN echo -n "-static-libgcc -static-libstdc++ -static" > /ldflags
RUN echo -n -all-static > /ltaldflags

RUN LDFLAGS="$(cat /ldflags)" LIBTOOL_APP_LDFLAGS="$(cat /ltaldflags)" ./autogen.sh

# flags
RUN if [ "$WALLET" = true ]; then \
        if [ "$USE_OLD_BERKLEYDB" = true ]; then \
            echo -n " -L/opt/db/lib/" >> /ldflags; \
            echo -n " -I/opt/db/include/" >> /cppflags; \
        else \
            echo -n --with-incompatible-bdb > /newdbflag; \
            echo -n " -I/usr/include/" >> /cppflags; \
        fi \
    else \
        echo -n --disable-wallet > /nowalletflag; \
    fi
RUN if [ "$UPNPC" = false ]; then \
        echo -n --without-miniupnpc > /noupnpcflag; \
    fi
RUN echo -n " -L/usr/lib/" >> /ldflags
RUN echo -n " -I/usr/include/boost/" >> /cppflags

RUN export LDFLAGS="$(cat /ldflags)" && \
    export CPPFLAGS="$(cat /cppflags)" && \
    export LIBTOOL_APP_LDFLAGS="$(cat /ltaldflags)" && \
    export NEW_DB="$(cat /newdbflag)" && \
    export WITHOUT_WALLET="$(cat /nowalletflag)" && \
    export WITHOUT_UPNPC="$(cat /noupnpcflag)" && \
    export CFLAGS="-static -static-libgcc -fno-strict-aliasing" && \
    eval "export $(dpkg-architecture | grep DEB_HOST_MULTIARCH)" && \
    ./configure \
        LDFLAGS="$LDFLAGS" \
        CPPFLAGS="$CPPFLAGS $CFLAGS" \
        CFLAGS="$CFLAGS" \
        LIBS=-ldl \
        "$NEW_DB" \
        "$WITHOUT_WALLET" \
        "$WITHOUT_UPNPC" \
        --prefix=/usr \
        --disable-tests \
        --disable-bench \
        --disable-ccache \
        --disable-shared \
        --with-boost-libdir=/usr/lib/"$DEB_HOST_MULTIARCH" \
        --without-gui

RUN make -j$(nproc --all) && \
    make install && \
    strip /usr/bin/mfcoind


FROM alpine

COPY --from=builder /usr/bin/mfcoind /usr/bin

RUN apk add --no-cache libc6-compat && \
    mkdir /data && \
    chown guest:nobody /data

USER guest

VOLUME [ "/data" ]

ENTRYPOINT [ "/usr/bin/mfcoind", "-datadir=/data" ]

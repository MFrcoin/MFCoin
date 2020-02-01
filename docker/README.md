# MFCoin daemon
This repository contains a dockerfile for mfcoin daemon

## Build by yourself
There is no special prerequisites for building standard image (contains wallet and databases procession based on berkleydb4.8 and firewall-jumping functionality). To build it enter main repository folder and run following:
```sh
$ docker build -f docker/Dockerfile .
```

### Args
Additionally you can use build args to customize the process:

#### VERSION
You can define what version to use. For example, `0.8.1.0` will switch git source tree to the branch `tags/v.0.8.1.0`. Default: `latest`
```sh
VERSION=0.8.1.0
```

#### WALLET
You can disable wallet functionality (useful for server nodes). Acceptable values = `true`, `false`. Default: `true`
```sh
WALLET=false
```

#### UPNPC
You can disable firewall-jumping functionality. Acceptable values = `true`, `false`. Default: `true`
```sh
UPNPC=false
```

#### USE_OLD_BERKLEYDB
Additionally, you can use the newest BerkleyDB distribution. This may increase build speed and daemon runtime performance and/or security (?) but will break daemon compatibility with `wallet.dat`'s based on old db. By default, daemon compiles **with** old db support, but you can redefine it. Acceptable values = `true`, `false`. Default: `true`
```sh
USE_OLD_BERKLEYDB=false
```

### Build example
Build minimal 0.8.1.0 version:
```sh
$ docker build --build-arg WALLET=false --build-arg UPNPC=false --build-arg VERSION=0.8.1.0 -f docker/Dockerfile .
```
In example above there is no difference between BerkleyDB versions — wallet isn't used so BerkleyDB isn't installed

### Build for a few architectures

#### Prerequisites

1. Docker [buildx](https://github.com/docker/buildx) command must be available.
1. Enable binfmt_misc by running <pre><code land="sh">$ docker run --rm --privileged docker/binfmt:66f9012c56a8316f9244ffd7622d7c21c1f6f28d</code></pre>
1. Create a new builder with multi-arch support <pre><code land="sh">$ docker buildx create --use --name mybuilder</code></pre> or swicth to existing <pre><code land="sh">$ docker buildx use mybuilder</code></pre>

#### Build process

In example above platforms defined as `linux/arm,linux/arm64,linux/amd64` and pushing to docker hub is executed immidiately (if you don't want so, just delete `--push`)

```sh
$ docker buildx build -f docker/Dockerfile -t YOUR/TAG --platform=linux/arm,linux/arm64,linux/amd64 . --push
```

## Run
You may feel free to use this image whatever you want, but there is need in mounting data volume to store daemon data outside the container. If you want it of course. In examples we will use `./data/daemon` as a permanent storage. Besides there is no need in special security or network flags, etc.

### Simply run container with permanent storage

#### Prerequisites

Directory on host machine (in this case ./data/daemon) must be available for reading, writing and executing by user with uid 405. Example:
```sh
$ mkdir -p data/daemon
$ chmod 700 data/daemon
$ sudo chown 405:nobody data/daemon
```
#### Run

```sh
$ docker run -it -v "$(pwd)"/data/daemon:/data kamehb/mfc-wallet-daemon
```

### Run minimal container as a rpc server
\* See running with permanent storage first
```sh
$ docker run -it -v "$(pwd)"/data/daemon:/data -p 22825:22825 kamehb/mfc-wallet-daemon:minimal -rpcport=22825 -rpcuser=RPC_USER -rpcpassword=RPC_PASS -reindex -txindex -rpcallowip=0.0.0.0/0
```
Note: running rpc server with mapping to host network is not needed in general. If it applicable just setup rpc server and services that using it on detached network. The simpliest way to achieve is described below in docker-compose config

### Run with docker-compose
There is simple config example for services that requires mfcoin daemon to work with
```yml
version: '3.3'

services:
  mfcoind:
    image: kamehb/mfc-wallet-daemon
    volumes:
      - ./data/daemon:/data
    command:
      - -rpcport=22825
      - -rpcuser=${RPC_USER}
      - -rpcpassword=${RPC_PASS}
      - -reindex
      - -txindex
      - -rpcallowip=0.0.0.0/0
  myservice:
    image: myimage
    volumes:
      - ./data/myservice:/data
    environment:
      - MFCOIND_USER=${RPC_USER}
      - MFCOIND_PASS=${RPC_PASS}
      - MFCOIND_PORT=22825
      - MFCOIND_HOST=mfcoind
```
Note: the best way to store user and password for rpc service in docker-compose is .env file. Do not forget to chmod it to 600

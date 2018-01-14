# Installing MFCoind from source :: CentOS 7

This Howto demonstrates how to compile MFCoind from source under CentOS 7 including
all dependencies, configure and run the node.

Assumption: all commands are executed under *root*

## Swap memory

In order for MFCoind to compile successfully, you need to make sure you have enough RAM and/or
sufficient swap space. If is recommended to have at least 1GB swap in any case, so to get it, run the
following commands on a fresh system once and it will make and configure swap and make sure it is
enabled during boot:

```
dd if=/dev/zero of=/swapfile bs=1024 count=1048576
chmod 0600 /swapfile
mkswap /swapfile
echo "/swapfile swap swap defaults 0 0" >> /etc/fstab
swapon /swapfile
```

The above will create 1GB swap space in /swapfile

## Dependancies

MFCoind requires few packages to compile and run. The below will setup custom repo with those (old) deps 
and will install them in the system:

```
yum install -y epel-release
yum install autoconf automake boost-devel gcc-c++ git libdb4-cxx libdb4-cxx-devel libevent-devel libtool openssl-devel wget miniupnpc-devel patch
```

_NOTE on boost_: Only the following are required from boost: _boost_system, boost_filesystem, boost_program_options, boost_thread_ and their deps,
so later on need to replace *libboost1.58-all-dev* with corresponding separate packages to bring less to the system

CentOS 7 comes with a version of OpenSSL lacking EC Libraries so we are going to have to download, build, and install a separate copy of OpenSSL.

```
wget https://www.openssl.org/source/openssl-1.0.1l.tar.gz
tar zxvf openssl-1.0.1l.tar.gz
cd openssl-1.0.1l
export CFLAGS="-fPIC"
./config --prefix=/opt/openssl shared enable-ec enable-ecdh enable-ecdsa
make all
make install
cd ../
rm -rf openssl-1.0.1l
```

## Prepare and build

Now as we are ready, let's fetch the MFCoin repo, do couple of tricks around to prepare and then build the MFCoind

```
git clone https://github.com/MFrcoin/MFCoin.git
cd MFCoin/src
make USE_UPNP=0 USE_IPV6=0 CXXFLAGS="-I/usr/include/libdb4 -L/usr/lib64/libdb4" -f makefile.unix
```

## Installation and Cleanup

As we don't really want to keep all those sources and have in order, lets make MFCoind discover-able in our PATH and clean the rest
 
```
mv MFCoind /usr/local/bin/
cd ../../
rm -rm MFCoin
```

## First Run

For MFCoind to run, we need to specify couple of config options, and those can be generated
automatically on first run:

```
MFCoind
```

The part of the out put will be:

```
rpcuser=MFCoinrpc
rpcpassword=4N3PdB1Day4W1rb59USU48LwfMX99XajiKwnbu1s7qBK
```

with *prcpassword* being some random string. Put both lines in *~/.MFCoind/MFCoin.conf*

## Daemon run

Now as we have all in order, each time you boot up, run the following command:

```
MFCoind -daemon
```

## ToDo

* Adjust the source code so we don't need to rename directories here and there during preparation task
* Write and commit to the code the init script that can make MFCoind run on startup
* Adjust Howto with installation of init script


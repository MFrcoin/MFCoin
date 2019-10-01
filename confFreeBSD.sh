#!/bin/sh -v
# Needed install from ports:
#	devel/libevent
#	devel/boost-libs
#	databases/db48
CPPFLAGS='-I/usr/local/include/db48/ -I/usr/local/include'
LDFLAGS='-L/usr/local/lib/db48/ -L/usr/local/lib'
CFLAGS=$CPPFLAGS
CXXFLAGS=$CPPFLAGS

export LDFLAGS CPPFLAGS CFLAGS CXXFLAGS

./configure --disable-tests --disable-dependency-tracking
#./configure --enable-debug
#./configure --enable-debug --with-libs 

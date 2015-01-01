#!/bin/bash

if ! [ $1 ]; then
   echo "Usage: $0 PACKAGE_ROOT"
   exit 1
fi

ROOT=$1
NAME="libfskit-fuse"
VERSION="0.$(date +%Y\%m\%d\%H\%M\%S)"

DEPS="libc6 libfskit libfuse2"

DEPARGS=""
for pkg in $DEPS; do
   DEPARGS="$DEPARGS -d $pkg"
done

source /usr/local/rvm/scripts/rvm

rm -f $NAME-0*.deb

fpm --force -s dir -t deb -a $(uname -m) -v $VERSION -n $NAME $DEPARGS -C $ROOT --license "LGPLv3+/ISC" --maintainer "Jude Nelson <judecn@gmail.com>" --url "https://github.com/jcnelson/fskit" --description "Fileserver utility library.  This package contains the FUSE bindings." $(ls $ROOT)


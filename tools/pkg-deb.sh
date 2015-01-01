#!/bin/bash

FSKIT_DIR=../
BUILD=./build
SCRIPTS=./pkg-scripts

build_package() {
   
   local dir=$1
   local build_target=$2
   local install_target=$3
   local package_script=$4
   local install_prefix=$5

   local install_dir=$BUILD/$target
   mkdir $install_dir

   if [ -n $build_target ]; then
      make -C $dir $build_target
   fi

   if [ -n $install_target ]; then
      mkdir -p $install_dir/$install_prefix
      make -C $dir $install_target PREFIX=$(realpath $install_dir/$install_prefix)
   fi
   
   if [ -n $package_script ]; then
      $SCRIPTS/$package_script $install_dir
   fi
}

rm -rf $BUILD

# build fskit 
build_package ../ libfskit libfskit-install pkg-fskit-deb.sh /usr/local

# build fskit-dev 
build_package ../ "" libfskit-dev-install pkg-fskit-dev-deb.sh /usr/local

# build fskit-fuse
build_package ../fuse libfskit_fuse libfskit_fuse-install pkg-fskit_fuse-deb.sh /usr/local

# build fskit-fuse-dev
build_package ../fuse "" libfskit_fuse-dev-install pkg-fskit_fuse-dev-deb.sh /usr/local

rm -rf $BUILD

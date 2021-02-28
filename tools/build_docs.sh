#!/bin/bash

MYSQL_LIB_DIR=$(pwd)
BUILD_DIR=$HOME/build

# Install doxygen
sudo apt install -y doxygen

# Install saxonhe
cd /tmp
wget -O saxonhe.zip https://sourceforge.net/projects/saxon/files/Saxon-HE/9.9/SaxonHE9-9-1-4J.zip/download
unzip -o saxonhe.zip
sudo rm /usr/share/java/Saxon-HE.jar
sudo cp saxon9he.jar /usr/share/java/Saxon-HE.jar

# Get Boost
mkdir -p $BUILD_DIR 
cd $BUILD_DIR
git clone https://github.com/boostorg/boost.git boost-root --depth 1
cd boost-root
export BOOST_ROOT=$(pwd)

# Initialize Boost
git submodule update --init libs/context
git submodule update --init tools/boostbook
git submodule update --init tools/boostdep
git submodule update --init tools/docca
git submodule update --init tools/quickbook
ln -s $MYSQL_LIB_DIR $BOOST_ROOT/libs/mysql
cp $MYSQL_LIB_DIR/tools/user-config.jam $HOME/
python tools/boostdep/depinst/depinst.py ../tools/quickbook
./bootstrap.sh
./b2 headers

# Build the docs
./b2 -j4 libs/mysql/doc//boostrelease

# Copy the resulting docs to a well-known location
cp -r libs/mysql/doc/html $MYSQL_LIB_DIR/build-docs
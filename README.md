# CSE 513 - Project 1 - Replicated Key-Value Store

Download grpc source and compile them as per the instructions online.

export MY_INSTALL_DIR=$HOME/.local

mkdir -p $MY_INSTALL_DIR

export PATH="$PATH:$MY_INSTALL_DIR/bin"

sudo apt install -y cmake

sudo apt install -y build-essential autoconf libtool pkg-config

sudo apt-get update & sudo apt-get install git


git clone --recurse-submodules -b v1.33.1 https://github.com/grpc/grpc & cd grpc

mkdir -p cmake/build

pushd cmake/build

cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..

make 

sudo make install

popd


Make sure to run `export PKG_CONFIG_PATH="<insert-path-to-grpc-libs>"`, before trying to run `make`

export PKG_CONFIG_PATH=/root/.local/lib/pkgconfig

sudo apt-get install build-essential autoconf libtool pkg-config

sudo apt-get install libssl-dev


All client changes are in client.h and all server changes are in server.cpp.

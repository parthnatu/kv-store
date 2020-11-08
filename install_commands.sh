export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
export PATH="$PATH:$MY_INSTALL_DIR/bin"

sudo apt-get update
sudo apt install -y cmake
sudo apt install -y build-essential autoconf libtool pkg-config

sudo apt-get install git
git clone --recurse-submodules -b v1.33.1 https://github.com/grpc/grpc

cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
make -j4
sudo make install
popd

# export PKG_CONFIG_PATH=/lib/pkgconfig
# sudo apt-get install build-essential autoconf libtool pkg-config
# sudo apt-get install libssl-dev

git clone https://github.com/parthnatu/kv-store.git
# CSE 513 - Project 1 - Replicated Key-Value Store

## How to run the client and server

Please export the appropriate PATH and PKG_CONFIG_PATH for your environment before running `make`.

Once the binaries for `Server` and `userprogram` are created follow the steps below

### Client

`./userprogram <CM/ABD>`

For any R/W ratio changes and server configuration changes, use userprogram.cpp.

`R/(R/W) ratio = NUM_READS/NUMBER_OF_CLIENTS`

Note for CM: The client refers to its own local server using its index in the `servers[]` array in userprogram.cpp. For example, if there are 5 entries in the array and there are 3 clients, client1 will refer to the 0th index, client2 to the 1st index, client3 to the 2nd index and the rest of the servers are global servers.

### Server

For ABD
`./Server <ip> <port> ABD`

For CM
`./Server <ip> <port> CM <server_config.txt>`

Note for CM: If running on gcloud, one complication that arises is that the external and internal IP are decoupled i.e. the server cannot bind to the external IP. To solve this, run,

`./Server 0.0.0.0 <port> CM <server_config.txt>`

The server will find its `self_index` in the server_config.txt by its unique ip:port. Make sure to replace its IP address in the file with `0.0.0.0 <port>`.

### Logs

Please create `logs/local` and `logs/local_cm` directories in the folder to store the logs for the client.

## How to setup the development environment (this is a sample for reference, please make changes specific to your system)

Download grpc source and compile them as per the instructions online.

export MY_INSTALL_DIR=$HOME/.local

mkdir -p $MY_INSTALL_DIR

export PATH="$PATH:$MY_INSTALL_DIR/bin" (protoc is here)

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

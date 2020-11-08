CXX = g++
CXXFLAGS =-Wall -std=c++14 -Iinc -g `pkg-config --cflags protobuf grpc`
LDFLAGS = -L/usr/local/lib -lm -ldl -lpthread `pkg-config --libs protobuf grpc++` -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = ./protos
vpath %.proto $(PROTOS_PATH)

# All the cpp file in the directory should be compiled
src = $(wildcard *.cpp)
obj = $(patsubst %.cpp, obj/%.o, $(src)) # Each cpp file will trun into an object file in the obj folder

obj2 = $(filter-out obj/userprogram.o,  $(obj)) # Object files required for the server application. It contains all the object files except the object file of userprogram which should be used for client
obj1 = $(filter-out obj/server.o, $(obj)) # Object files for userprogram application. Please note that Client application created by this Makefile will be the one that runs the main function in the userprogram object file

.PHONY: all
all: obj Client Server

client : obj Client
Client: kvmsg.pb.o kvmsg.grpc.pb.o $(obj1)
	$(CXX) -o $@ $^ $(LDFLAGS) # Link object files and create the client application

server: obj Server
Server: kvmsg.pb.o kvmsg.grpc.pb.o $(obj2)
	$(CXX) -o $@ $^ $(LDFLAGS) # Link object files and create the server application

obj:
	mkdir obj # Create a folder for the object files

# Compile the code and create object files
$(obj): obj/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

.PHONY: clean
clean:
	rm -rf *.o *.pb.cc *.pb.h obj Client Server

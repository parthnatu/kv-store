CXX = g++
CXXFLAGS =-Wall -std=c++11 -Iinc -g `pkg-config --cflags protobuf grpc`
LDFLAGS = -L/usr/local/lib -lm -ldl -lpthread `pkg-config --libs protobuf grpc++` -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed

# All the cpp file in the directory should be compiled
src = $(wildcard *.cpp)
obj = $(patsubst %.cpp, obj/%.o, $(src)) # Each cpp file will trun into an object file in the obj folder

obj2 = $(filter-out obj/userprogram.o,  $(obj)) # Object files required for the server application. It contains all the object files except the object file of userprogram which should be used for client
obj1 = $(filter-out obj/server.o, $(obj)) # Object files for userprogram application. Please note that Client application created by this Makefile will be the one that runs the main function in the userprogram object file

.PHONY: all
all: obj Client Server

client : obj Client
Client: $(obj1)
	$(CXX) -o $@ $^ $(LDFLAGS) # Link object files and create the client application

server: obj Server
Server: $(obj2)
	$(CXX) -o $@ $^ $(LDFLAGS) # Link object files and create the server application

obj:
	mkdir obj # Create a folder for the object files

# Compile the code and create object files
$(obj): obj/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf obj Client Server


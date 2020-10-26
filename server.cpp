#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <map>
#include <utility>

#include "kvmsg.grpc.pb.h"

using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::ReadRequest;
using kvstore::ReadReply;
using kvstore::WriteRequest;
using kvstore::WriteReply;
using kvstore::KVStore;

map<string, pair<string, int>> store;

// Logic and data behind the server's behavior.
class KVServerImpl final : public KVStore::Service {
  Status read(ServerContext* context, const ReadRequest* request,
                  ReadReply* reply) override {
    //std::string prefix("Hello ");
    //reply->set_message(prefix + request->name());
    return Status::OK;
  }

  Status write(ServerContext* context, const WriteRequest* request,
                  WriteReply* reply) override {
    //std::string prefix("Hello again ");
    //reply->set_message(prefix + request->name());
    return Status::OK;
  }
};

void RunServer(string &server_address) {
  KVServerImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}


int main(int argc, char *argv[]){
	if (argc != 2) {
		fprintf(stderr, "%s%s%s\n", "Error\n"
		"Usage: ", argv[0], " <ip:port>\n\n"
		"Please note to run the servers first\n");
                return -1;
        } 

	std::string serverAddress = std::string(argv[1]);
	RunServer(serverAddress);

	return 0;
}

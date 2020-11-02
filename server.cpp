#include <iostream>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <queue>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <map>
#include <utility>
#include <thread>
#include <mutex>

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
using kvstore::CM;
using kvstore::CMRecRequest;
using kvstore::CMRecReply;

struct CMTuple{
    uint32_t id;
    uint32_t timestamp; 
    string key; 
    string value; 
};

map<string, pair<string, int>> abd_store;
map<string, string> cm_store;
vector<int> ts_cm;
map<string, string> server_info;
mutex mtx;
queue<CMTuple*> inqueue;
queue<CMTuple*> outqueue;

// Logic and data behind the server's behavior.
class KVServerImpl final : public KVStore::Service {
  Status read(ServerContext* context, const ReadRequest* request,
                  ReadReply* reply) override {
    string prot = (string)request->prot();
    if(prot == "CM"){
      mtx.lock();
      mtx.unlock();
    }
    else{}
    return Status::OK;
  }

  Status write(ServerContext* context, const WriteRequest* request,
                  WriteReply* reply) override {
    string prot = (string)request->prot();
    if(prot == "CM"){
      mtx.lock();
      mtx.unlock();
    }
    else{}
    return Status::OK;
  }
};

class CMImpl final : public CM::Service {
  Status receive(ServerContext* context, const CMRecRequest* request,
                  CMRecReply* reply) override {
    mtx.lock();
    CMTuple* tup = new CMTuple();
    tup->id = (uint32_t)request->id();
    tup->timestamp = (uint32_t)request->timestamp();
    tup->key = (string)request->key();
    tup->value = (string)request->value();
    reply->set_code(200);
    inqueue.push(tup);
    mtx.unlock();
    return Status::OK;
  }
};

void send(){
  mtx.lock();
  mtx.unlock();
}

void apply(){
  mtx.lock();
  mtx.unlock();
}

void RunServer(string &server_address, string protocol) {
  KVServerImpl kvservice;
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&kvservice);
  // Finally assemble the server.
  if(protocol == "CM"){
    CMImpl cmservice;
    builder.RegisterService(&cmservice);
    thread send_t(send);
    thread apply_t(apply);
  }
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}



int main(int argc, char *argv[]){
  /*if (argc != 2) {
		fprintf(stderr, "%s%s%s\n", "Error\n"
		"Usage: ", argv[0], " <ip:port>\n\n"
		"Please note to run the servers first\n");
                return -1;
		} */

	std::string serverAddress = std::string(argv[1]);
	std::string port = std::string(argv[2]);
	std::string protocol = std::string(argv[3]);
	if(protocol == "CM"){
	  std::string inputfile = std::string(argv[4]);
	  ifstream myfile(inputfile);
	  string line;
	  while (getline(myfile,line)){
	    string ip = line.substr(0,line.find(" "));
	    string port = line.substr(line.find(" "),line.length()-1);
	    server_info.insert(pair<string,string>(ip,port));
	  }
	}
	RunServer(serverAddress, protocol);

	return 0;
}

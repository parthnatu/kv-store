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
using kvstore::TagRequest;
using kvstore::TagReply;
using kvstore::KVStore;
using kvstore::CM;
using kvstore::CMRecRequest;
using kvstore::CMRecReply;
using grpc::Channel;
using grpc::ChannelInterface;
using grpc::ClientContext;
using grpc::Status;

struct CMTuple{
  uint32_t id;
  vector<int> timestamp;
  string key;
  string value; 
};
struct ValueWithtag {
	string value;
	uint32_t tag;
	uint32_t fromClientId;
	pthread_mutex_t lock;

	ValueWithtag(string &_value, uint32_t _tag, uint32_t _fromClientId, pthread_mutex_t &_lock): 
						value(_value), tag(_tag), fromClientId(_fromClientId), lock(_lock) {
	}
};
string prot;
uint32_t cm_self_index;
uint32_t cm_N; // number of nodes
map<string, pair<string, int>> abd_store;
map<string, string> cm_store;
vector<int> ts_cm;
vector<pair<string, string>> server_info;
mutex mtx;
queue<CMTuple*> inqueue;
queue<CMTuple*> outqueue;
map<string, ValueWithtag> local_db;
uint32_t tag = 1;	// start server tag from 1
pthread_mutex_t global_lock, tag_lock;
static bool isBefore(CMTuple* ts1, CMTuple* ts2) {
        bool isBefore = false;
        for (int i = 0; i < cm_N; i++) {
            int cmp = ts1->timestamp[i] - ts2->timestamp[i];
            if (cmp > 0)
              return false; // note, could return false even if isBefore is true
            else if (cmp < 0)
              isBefore = true;
        }
        return isBefore;
}
// Logic and data behind the server's behavior.
class KVServerImpl final : public KVStore::Service {

  /*bool isBefore(CMTuple* ts1, CMTuple* ts2) {
        bool isBefore = false;
        for (int i = 0; i < cm_N; i++) {
            int cmp = ts1->timestamp[i] - ts2->timestamp[i];
            if (cmp > 0)
              return false; // note, could return false even if isBefore is true
            else if (cmp < 0)
              isBefore = true;
        }
        return isBefore;
	}*/
  Status read(ServerContext* context, const ReadRequest* request,
                  ReadReply* reply) override {

    if(prot == "CM"){
      mtx.lock();
      reply->set_value(cm_store[request->key()]);
      mtx.unlock();
    }
    else{}
    return Status::OK;
  }

  Status write(ServerContext* context, const WriteRequest* request,
                  WriteReply* reply) override {
    if(prot == "CM"){
      mtx.lock();

      uint32_t client_id = (uint32_t) request->client_id();
      ts_cm[client_id] += 1;
      string key = (string) request->key();
      string value = (string) request->value();
      cm_store[key] = value;
      CMTuple* tup = new CMTuple();
      tup->id = client_id;
      tup->timestamp = ts_cm;
      tup->key = key;
      tup->value = value;
      vector<CMTuple*> tmp;
      while(!outqueue.empty())
      {
	tmp.push_back(outqueue.front());
	outqueue.pop();
      }
      tmp.push_back(tup);
      sort(tmp.begin(),tmp.end(),isBefore);
      for(int i=0;i<tmp.size();i++){
	outqueue.push(tmp[i]);
      }
      mtx.unlock();
    }
    else{}
    return Status::OK;
  }
};

class KVServerABDImpl final: public KVStore::Service
{
private:
	inline bool break_tie(uint32_t &localTag, uint32_t &localId, 
			uint32_t &clientTag, uint32_t &clientId) {
		return (localTag < clientTag || (localTag == clientTag && localId < clientId));
	}

public:
	Status getServerTag(ServerContext *context, const TagRequest *request,
			TagReply *reply) override
	{
		reply->set_tag(tag);
		return Status::OK;
	}

	Status read(ServerContext *context, const ReadRequest *request,
			ReadReply *reply) override
	{
		string key = request->key();

		auto it = local_db.find(key);
		if (it != local_db.end())
		{
			string value = it->second.value;
			uint32_t localTag = it->second.tag;
			reply->set_value(value);
			reply->set_timestamp(localTag);
		}
		else 
		{
			reply->set_value("nil");
			reply->set_timestamp(0);
		}
		return Status::OK;
	}

	Status write(ServerContext *context, const WriteRequest *request,
			WriteReply *reply) override
	{
		string key = request->key();
		string value = request->value();
		uint32_t clientId = request->client_id();
		uint32_t clientTag = request->timestamp();

		auto it = local_db.find(key);
		
		if (it == local_db.end()) 
		{
			pthread_mutex_lock(&global_lock);

			it = local_db.find(key);
			if (it == local_db.end()) {
				pthread_mutex_t lock;
				pthread_mutex_init(&lock, NULL);
				
				ValueWithtag vtag(value, 0, 100, lock);	// random
				auto insert_res = local_db.insert({key, vtag});
				
				it = insert_res.first;
			}
			
			pthread_mutex_unlock(&global_lock);
		}

		bool allow_write = break_tie(it->second.tag, it->second.fromClientId, clientTag, clientId);
		if (allow_write) 
		{
			pthread_mutex_lock(&it->second.lock);
			
			allow_write = break_tie(it->second.tag, it->second.fromClientId, clientTag, clientId);
			if (allow_write) {
				it->second.value = value;
				it->second.fromClientId = clientId;
				it->second.tag = clientTag;
				
				// increament tag after write operation
				pthread_mutex_lock(&tag_lock);
				tag = max(tag, clientTag) + 1;
				pthread_mutex_unlock(&tag_lock);
			}
			
			pthread_mutex_unlock(&it->second.lock);
		}

		reply->set_ack(1);

		return Status::OK;
	}
};

class CMImpl final : public CM::Service {
  /*public bool isBefore(CMTuple* ts1, CMTuple* ts2) {
        boolean isBefore = false;
        for (int i = 0; i < cm_N; i++) {
            int cmp = ts1->timestamp[i] - ts2->timestamp[i];
            if (cmp > 0)
              return false; // note, could return false even if isBefore is true
            else if (cmp < 0)
              isBefore = true;
        }
        return isBefore;
	}*/
  Status receive(ServerContext* context, const CMRecRequest* request,
                  CMRecReply* reply) override {
    mtx.lock();
    CMTuple* tup = new CMTuple();
    tup->id = (uint32_t)request->id();
    vector<int> timestamp;
    for(uint32_t i=0;i<cm_N;i++){
      timestamp.push_back(request->timestamp(i));
    }
    tup->timestamp = timestamp;
    tup->key = (string)request->key();
    tup->value = (string)request->value();
    reply->set_code(200);
    vector<CMTuple*> tmp;
    while(!inqueue.empty())
    {
      tmp.push_back(inqueue.front());
      inqueue.pop();
    }
    tmp.push_back(tup);
    sort(tmp.begin(),tmp.end(),isBefore);
    for(int i=0;i<tmp.size();i++){
      inqueue.push(tmp[i]);
    }
    mtx.unlock();
    return Status::OK;
  }
};

class CMClientImpl{
public:
  CMClientImpl(std::shared_ptr<ChannelInterface> channel): stub_(CM::NewStub(channel)) {}

  int receive(CMTuple* tup){
    CMRecRequest request;
    request.set_id(tup->id);
    request.set_key(tup->key);
    request.set_value(tup->value);
    *request.mutable_timestamp() = {tup->timestamp.begin(), tup->timestamp.end()};
    //for(uint32_t i=0;i<cm_N;i++){
    //  request.set_timestamp(i,tup->timestamp[i]);
    //}
    CMRecReply reply;

    ClientContext context;

    // The actual RPC.
    Status status = stub_->receive(&context, request, &reply);

    if (status.ok()) {
      return 0;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }
  private:
	std::unique_ptr<CM::Stub > stub_;
};

void send(){
  while(true){
    mtx.lock();
    if(!outqueue.empty()){
      
      for(uint32_t i=0;i<cm_N;i++){
	if(i != cm_self_index){
	  string target_str = server_info[i].first + ":" + server_info[i].second;
	  CMClientImpl recv_call(grpc::CreateChannel(
      target_str, grpc::InsecureChannelCredentials()));
	  //mtx.lock();
	  CMTuple* tup = outqueue.front();
	  mtx.unlock();
	  recv_call.receive(tup);
	  mtx.lock();
	}
      }
      //mtx.lock();
      outqueue.pop();
    }
    mtx.unlock();
  }
}

void apply(){
  while(true){
    mtx.lock();
    if(!inqueue.empty()){

      CMTuple* tup = inqueue.front();
     
      bool flag = true;
      for(uint32_t i=0;i<cm_N;i++){
	if(i != tup->id){
	  if(tup->timestamp[i] <= ts_cm[i])
	    continue;
	  else{
	    flag = false;
	    break;
	  } 
	}
      }
      if(flag && tup->timestamp[tup->id] == ts_cm[tup->id] + 1){
	inqueue.pop();
	ts_cm[tup->id] = tup->timestamp[tup->id];
	cm_store[tup->key] = tup->value;
      }
    }
    mtx.unlock();
  }

}

void RunServer(string &server_address, string protocol) {
  KVServerImpl kvservice;
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  if (pthread_mutex_init(&global_lock, NULL) != 0) {
    cout << "\n mutex init has failed\n";
    return;
  }
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
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    for(int i=0;i<server_info.size();i++){
      ts_cm.push_back(0);
    }
    thread send_t(send);
    thread apply_t(apply);
    send_t.join();
    apply_t.join();
    server->Wait();
  }
  else{
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    KVServerABDImpl service;
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
    // destroy locks
    for (auto it = local_db.begin(); it != local_db.end(); it++) {
      pthread_mutex_destroy(&it->second.lock);
    }
    pthread_mutex_destroy(&global_lock);
    // clear local key-value database
    local_db.clear();
  }
}



int main(int argc, char *argv[]){
  if (argc < 4) {
		fprintf(stderr, "%s%s%s\n", "Error\n"
		"Usage: ", argv[0], " <ip> <port> <protocol> <server_list.txt>\n\n"
		"Please note to run the servers first\n");
                return -1;
		} 
  cout << argc << "\n";
  std::string serverAddress = std::string(argv[1]);
  std::string port = std::string(argv[2]);
  std::string protocol = std::string(argv[3]);
  prot = protocol;
  if(protocol == "CM"){
    std::string inputfile = std::string(argv[4]);
    ifstream myfile(inputfile);
    string line;
    uint32_t index = 0;
    while (getline(myfile,line)){
      string _ip = line.substr(0,line.find(" "));
      string _port = line.substr(line.find(" ")+1,line.length());
      if(serverAddress.compare(_ip) == 0 && port.compare(_port) == 0)
	cm_self_index = index;
      index++;
      server_info.push_back(pair<string,string>(_ip,_port));
    }
    cm_N = index;
  }
  serverAddress = serverAddress+":"+port;
  RunServer(serverAddress, protocol);
  return 0;
}

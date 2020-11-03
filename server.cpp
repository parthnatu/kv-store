#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <utility>
#include <pthread.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
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

struct ValueWithtag {
	string value;
	uint32_t tag;
	uint32_t fromClientId;
	pthread_mutex_t lock;

	ValueWithtag(string &_value, uint32_t _tag, uint32_t _fromClientId, pthread_mutex_t &_lock): 
						value(_value), tag(_tag), fromClientId(_fromClientId), lock(_lock) {
	}
};

map<string, ValueWithtag> local_db;
uint32_t tag = 1;
pthread_mutex_t global_lock, tag_lock;

class KVServerImpl final: public KVStore::Service
{
private:
	bool break_tie(uint32_t &localTag, uint32_t &localId, 
			uint32_t &clientTag, uint32_t &clientId) {
		if (clientTag < localTag) {
			return false;
		}
		else if (clientTag == localTag && clientId < localId) {
			return false;
		}
		else {
			return true;
		}
	}

public:
	Status gettag(ServerContext *context, const TagRequest *request,
			TagReply *reply) override
	{
		reply->set_tag(tag);
		cout << "gettag:: sending tag " << tag << " to client " << request->client_id() << endl;
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
			return Status::OK;
		}
		else 
		{
			reply->set_value("");
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

		cout << "write:: request from client " << clientId;
		cout << " " << key << "," << clientTag << "\n";

		auto it = local_db.find(key);
		
		if (it == local_db.end()) 
		{
			pthread_mutex_lock(&global_lock);
			it = local_db.find(key);
			if (it == local_db.end()) {
				pthread_mutex_t lock;
				pthread_mutex_init(&lock, NULL);
				ValueWithtag vtag(value, 0, 100, lock);	// random
				local_db.insert({key, vtag});
			}
			pthread_mutex_unlock(&global_lock);
		}

		cout << "write:: client << " << clientId << " existence check done\n";

		it = local_db.find(key);

		bool allow_write = break_tie(it->second.tag, it->second.fromClientId, clientTag, clientId);
		if (allow_write) 
		{
			cout << "write:: client << " << clientId << " allow write check1\n";
			pthread_mutex_lock(&it->second.lock);
			allow_write = break_tie(it->second.tag, it->second.fromClientId, clientTag, clientId);
			cout << "write:: client << " << clientId << " allow write check2 is " << allow_write << endl;
			if (allow_write) {
				it->second.value = value;
				it->second.fromClientId = clientId;
				it->second.tag = clientTag;
				pthread_mutex_lock(&tag_lock);
				tag = max(tag, clientTag) + 1;
				pthread_mutex_unlock(&tag_lock);
			}
			pthread_mutex_unlock(&it->second.lock);
		}

		cout << "write:: send ack to client " << clientId << endl;
		reply->set_ack(1);

		return Status::OK;
	}
};

void RunServer(string & server_address)
{
	if (pthread_mutex_init(&global_lock, NULL) != 0) {
		cout << "\n mutex init has failed\n";
		return;
	}

	KVServerImpl service;

	grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();
	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "%s%s%s\n", "Error\n"
			"Usage: ", argv[0], "<ip:port>\n\n"
			"Please note to run the servers first\n");
		return -1;
	}

	std::string serverAddress = std::string(argv[1]);
	RunServer(serverAddress);

	return 0;
}

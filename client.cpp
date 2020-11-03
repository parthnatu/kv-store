#include "client.h"
#include <ctime>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "kvmsg.grpc.pb.h"

using namespace std;

// using namespace grpc;
using grpc::Channel;
using grpc::ChannelInterface;
using grpc::ClientContext;
using grpc::Status;

// using namespace kvstore
using kvstore::ReadRequest;
using kvstore::ReadReply;
using kvstore::WriteRequest;
using kvstore::WriteReply;
using kvstore::TagRequest;
using kvstore::TagReply;
using kvstore::KVStore;

class KVClient
{
public:
	KVClient(std::shared_ptr<ChannelInterface> channel): stub_(KVStore::NewStub(channel)) {}

	uint32_t gettag(const string &key, const int client_id)
	{
		TagRequest request;
		request.set_client_id(client_id);

		TagReply reply;

		ClientContext context;

		Status status = stub_->gettag(&context, (TagRequest &) request, &reply);

		if (status.ok())
		{
			return reply.tag();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() <<
				std::endl;
			return 0;
		}
	}

	string read(const string &key, const int client_id)
	{
		ReadRequest request;
		request.set_key(key);
		request.set_client_id(client_id);

		ReadReply reply;

		ClientContext context;

		Status status = stub_->read(&context, (ReadRequest &) request, &reply);

		if (status.ok())
		{
			// write read logic here
			return "";
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() <<
				std::endl;
			return "RPC failed";
		}
	}

	int write(const string &key, const string &value, const int client_id, const int timestamp)
	{
		// Data we are sending to the server.
		WriteRequest request;
		request.set_key(key);
		request.set_client_id(client_id);
		request.set_value(value);
		request.set_timestamp(timestamp);

		// Container for the data we expect from the server.
		WriteReply reply;

		// Context for the client. It could be used to convey extra information to
		// the server and/or tweak certain RPC behaviors.
		ClientContext context;

		// The actual RPC.
		Status status = stub_->write(&context, (WriteRequest &) request, &reply);

		// Act upon its status.
		if (status.ok())
		{
			// write logic here
			return 0;
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() <<
				std::endl;
			return -1;
		}
	}

	private:
		std::unique_ptr<KVStore::Stub > stub_;
};

struct Client* client_instance(const uint32_t id, const char *protocol, const struct Server_info *servers, uint32_t number_of_servers)
{
	Client *client = new Client();
	client->id = id;
	if (protocol == "ABD")
	{
		for (int i = 0; i < 3; i++)
			client->protocol[i] = protocol[i];
		client->protocol[3] = '\0';
	}
	else
	{
		for (int i = 0; i < 2; i++)
			client->protocol[i] = protocol[i];
		client->protocol[2] = '\0';
	}
	client->servers = servers;
	client->number_of_servers = number_of_servers;
	cout << "initialized " << client->protocol << " client\n";
	return client;
}

int put(const struct Client *c, const char *key, uint32_t key_size, const char *value, uint32_t value_size)
{
	string _key = string(key);
	string _value = string(value);

	for (uint32_t i=0; i < c->number_of_servers; i++) {
		string serverIP = string(c->servers[i].ip);
		KVClient client = KVClient(
			grpc::CreateChannel(serverIP, grpc::InsecureChannelCredentials())
		);

		// client->write(key, value, c->id, timestamp);
	}
	return 0;
}

int get(const struct Client *c, const char *key, uint32_t key_size, char **value, uint32_t *value_size)
{
	return 0;
}

int client_delete(struct Client *c)
{
	delete c;
	return 0;
}


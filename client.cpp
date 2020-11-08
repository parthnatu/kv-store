#include "client.h"
#include <thread>
#include <future>
#include <queue>
#include <mutex>

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


struct valueWithTag {
	string value;
	uint32_t tag;

	valueWithTag(string _value, uint32_t _tag): value(_value), tag(_tag) {}
};


class KVServerChannel
{
public:
	KVServerChannel(std::shared_ptr<Channel> channel): stub_(KVStore::NewStub(channel)) {}

	uint32_t getServerTag(const uint32_t client_id)
	{
		TagRequest request;
		request.set_client_id(client_id);

		TagReply reply;

		ClientContext context;
		Status status = stub_->getServerTag(&context, (TagRequest &) request, &reply);

		if (status.ok()) {
			return reply.tag();
		} else {
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 0;
		}
	}

	valueWithTag read(const string &key, const uint32_t client_id)
	{
		ReadRequest request;
		request.set_key(key);
		request.set_client_id(client_id);

		ReadReply reply;

		ClientContext context;
		Status status = stub_->read(&context, (ReadRequest &) request, &reply);

		if (status.ok()) {
			return valueWithTag(reply.value(), reply.timestamp());
		} else {
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return valueWithTag(string("RPC failed"), 0);
		}
	}

	int write(const string &key, const string &value, const uint32_t client_id, const uint32_t timestamp)
	{
		WriteRequest request;
		request.set_key(key);
		request.set_client_id(client_id);
		request.set_value(value);
		request.set_timestamp(timestamp);

		WriteReply reply;

		ClientContext context;
		Status status = stub_->write(&context, (WriteRequest &) request, &reply);

		if (status.ok()) {
			return 0;
		} else {
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return -1;
		}
	}

	private:
		std::unique_ptr<KVStore::Stub > stub_;
};


string getServerAddr(const string &ip, const string &port) {
	return ip + ":" + port;
}

template<typename R>
bool is_ready(std::future<R> const &f) {
	return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}


/*
 * helper functions functions - protocol independent
 */

KVServerChannel* getKVServerChannel(string serverAddr) {
	KVServerChannel *client = new KVServerChannel(
		grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials())
	);
	return client;
}

vector<KVServerChannel*> getKVServerChannelsAsync(const struct Client *c) {
	vector<KVServerChannel*> clients;

	// create clients async
	vector<std::future<KVServerChannel*>> futures;
	for (uint32_t i = 0; i<c->number_of_servers; i++) {
		string serverAddr = getServerAddr(c->servers[i].ip, to_string(c->servers[i].port));
		std::future<KVServerChannel*> fu = std::async(std::launch::async, getKVServerChannel, serverAddr);
		futures.push_back(std::move(fu));
	}

	for (uint32_t i = 0; i<c->number_of_servers; i++) {
		KVServerChannel* client = futures[i].get();
		clients.push_back(client);
	}

	return clients;
}

void getTagFromServer(KVServerChannel* channel, const uint32_t client_id, 
			promise<uint32_t> &tag_promise) {
	uint32_t tag = channel->getServerTag(client_id);
	try {
		tag_promise.set_value(tag);
	} catch (const std::future_error& e) {}
}

void writeToServer(KVServerChannel *channel, const string &key, const string &value, 
			const uint32_t client_id, const uint32_t timestamp, promise<int> &write_promise) {
	int status = channel->write(key, value, client_id, timestamp);

	try {
		write_promise.set_value(status);
	} catch (const std::future_error& e) {}

	return;
}

void readFromServer(KVServerChannel *channel, const string &key, const uint32_t client_id, 
			promise<valueWithTag> &read_promise) {
	valueWithTag reply = channel->read(key, client_id);

	try {
		read_promise.set_value(reply);
	} catch (const std::future_error& e) {}

	return;
}


/*
 * ABD functions
 */

uint32_t getTagFromMajorityAsync(vector<KVServerChannel*> &channels, const struct Client *c) {
	int n_channels = (int)channels.size();

	queue<std::promise<uint32_t>> tag_promises;
	queue<std::future<uint32_t>> tag_futures;
	for (int i = 0; i<n_channels; i++) {
		std::promise<uint32_t> pr;
		std::future<uint32_t> fu = pr.get_future();
		std::async(std::launch::async, getTagFromServer, channels[i], c->id, ref(pr));
		tag_promises.push(std::move(pr));
		tag_futures.push(std::move(fu));
	}

	uint32_t tag = 0;

	int n_majority = n_channels / 2 + 1;
	int n_got = 0;
	while (n_got < n_majority) {
		std::future<uint32_t> fu = std::move(tag_futures.front()); tag_futures.pop();
		std::promise<uint32_t> pr = std::move(tag_promises.front()); tag_promises.pop();
		if (is_ready(fu)) {
			tag = max(tag, fu.get());
			n_got++;
		}
		else {
			tag_futures.push(std::move(fu));
			tag_promises.push(std::move(pr));
		}
	}

	while (n_got < n_channels) {
		std::future<uint32_t> fu = std::move(tag_futures.front()); tag_futures.pop();
		std::promise<uint32_t> pr = std::move(tag_promises.front()); tag_promises.pop();
		if (!is_ready(fu)) {
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		}
		fu.get();
		n_got++;
	}

	return tag;
}

bool writeToServersAsync(vector<KVServerChannel*> &channels, const string &key, const string &value, 
			const uint32_t client_id, const uint32_t timestamp) {
	int n_channels = (int)channels.size();

	queue<promise<int>> promises;
	queue<future<int>> futures;

	for (int i = 0; i<n_channels; i++) {
		promise<int> pr;
		future<int> fu = pr.get_future();
		async(std::launch::async, writeToServer, channels[i], key, value, client_id, timestamp, ref(pr));
		promises.push(std::move(pr));
		futures.push(std::move(fu));
	}

	int n_majority = n_channels / 2 + 1;
	int n_got = 0;
	while (n_got < n_majority) {
		future<int> fu = std::move(futures.front()); futures.pop();
		promise<int> pr = std::move(promises.front()); promises.pop();
		if (is_ready(fu)) {
			fu.get(); // assuming no failure
			n_got++;
		}
		else {
			futures.push(std::move(fu));
			promises.push(std::move(pr));
		}
	}

	while (n_got++ < n_channels) {
		future<int> fu = std::move(futures.front()); futures.pop();
		promise<int> pr = std::move(promises.front()); promises.pop();
		if (!is_ready(fu)) {
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		}
		fu.get(); 
	}

	return true;
}

string readFromServersAsync(vector<KVServerChannel*> &channels, const string &key, const uint32_t client_id) {
	int n_channels = (int)channels.size();

	queue<std::promise<valueWithTag>> promises;
	queue<std::future<valueWithTag>> futures;

	for (int i = 0; i<n_channels; i++) {
		std::promise<valueWithTag> pr;
		std::future<valueWithTag> fu = pr.get_future();
		std::async(std::launch::async, readFromServer, channels[i], key, client_id, ref(pr));
		promises.push(std::move(pr));
		futures.push(std::move(fu));
	}

	string value = "";
	uint32_t highestTag = 0;

	int n_majority = n_channels / 2 + 1;
	int n_got = 0;
	while (n_got < n_majority) {
		std::future<valueWithTag> fu = std::move(futures.front()); futures.pop();
		std::promise<valueWithTag> pr = std::move(promises.front()); promises.pop();
		if (is_ready(fu)) {
			valueWithTag reply = fu.get();
			if (reply.tag > highestTag) {
				highestTag = reply.tag;
				value = reply.value;
			}
			n_got++;
		}
		else {
			futures.push(std::move(fu));
			promises.push(std::move(pr));
		}
	}

	while (n_got < n_channels) {
		std::future<valueWithTag> fu = std::move(futures.front()); futures.pop();
		std::promise<valueWithTag> pr = std::move(promises.front()); promises.pop();
		if (!is_ready(fu)) {
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		}
		fu.get();
		n_got++;
	}

	return value;
}


/*
 * Functions called by userprogram.cpp
 */

struct Client* client_instance(const uint32_t id, const char *protocol, const struct Server_info *servers, uint32_t number_of_servers)
{
	Client *client = new Client();
	client->id = id;
	if (strcmp(protocol,"ABD") == 0) {
		for (int i = 0; i < 3; i++) client->protocol[i] = protocol[i];
		client->protocol[3] = '\0';
	}
	else {
		for (int i = 0; i < 2; i++) client->protocol[i] = protocol[i];
		client->protocol[2] = '\0';
	}
	client->servers = servers;
	client->number_of_servers = number_of_servers;

	return client;
}


int put(const struct Client *c, const char *key, uint32_t key_size, const char *value, uint32_t value_size)
{
	vector<KVServerChannel*> channels = getKVServerChannelsAsync(c);

	if (strcmp(c->protocol, "ABD") == 0) {
		string _key = string(key);
		string _value = string(value);
		while (_value.length() > 1024)
			_value.pop_back(); // avoid the extra newline character

		int highestTag = getTagFromMajorityAsync(channels, c);
		writeToServersAsync(channels, _key, _value, c->id, highestTag + 1);
	}

	return 0;
}


int get(const struct Client *c, const char *key, uint32_t key_size, char **value, uint32_t *value_size)
{
	vector<KVServerChannel*> channels = getKVServerChannelsAsync(c);
	
	if (strcmp(c->protocol, "ABD") == 0) {
		string valueStr = readFromServersAsync(channels, string(key), c->id);

		*value_size = valueStr.length();

		*value = new char[*value_size];
		valueStr.copy(*value, *value_size);
	}

	return 0;
}


int client_delete(struct Client *c)
{
	delete c;
	return 0;
}


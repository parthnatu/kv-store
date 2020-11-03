#include "client.h"
#include <ctime>
#include <thread>
#include <future>
#include <queue>

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


class KVClient
{
public:
	KVClient(std::shared_ptr<Channel> channel): stub_(KVStore::NewStub(channel)) {}

	uint32_t gettag(const uint32_t client_id)
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

	valueWithTag read(const string &key, const uint32_t client_id)
	{
		ReadRequest request;
		request.set_key(key);
		request.set_client_id(client_id);

		ReadReply reply;

		ClientContext context;

		Status status = stub_->read(&context, (ReadRequest &) request, &reply);

		if (status.ok())
		{
			return valueWithTag(reply.value(), reply.timestamp());
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() <<
				std::endl;
			string error = "RPC failed";
			return valueWithTag(error, 0);
		}
	}

	int write(const string &key, const string &value, const uint32_t client_id, const uint32_t timestamp)
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


string getServerAddr(const string &ip, const string &port) {
	return ip + ":" + port;
}

template<typename R>
bool is_ready(std::future<R> const &f) {
	return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}


KVClient* getKVClient(string serverAddr) {
	cout << "\tgetKVClient:: create channel to server " << serverAddr << endl;
	KVClient *client = new KVClient(
		grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials())
	);
	return client;
}


vector<KVClient*> getKVClientsAsync(const struct Client *c) {
	vector<KVClient*> clients;

	// create clients async
	vector<std::future<KVClient*>> create_futures;
	for (uint32_t i = 0; i<c->number_of_servers; i++) {
		string serverAddr = getServerAddr(c->servers[i].ip, to_string(c->servers[i].port));
		std::future<KVClient*> create_future = std::async(std::launch::async, getKVClient, serverAddr);
		create_futures.push_back(std::move(create_future));
	}

	// get clients from future
	for (uint32_t i = 0; i<c->number_of_servers; i++) {
		KVClient* client = create_futures[i].get();
		clients.push_back(client);
	}

	return clients;
}


void getTagFromServer(KVClient* channel, const uint32_t client_id, promise<uint32_t> &tag_promise) {
	// cout << "\tgetTagFromServer:: Get tag for client " << client_id << endl;
	uint32_t tag = channel->gettag(client_id);
	try {
		tag_promise.set_value(tag);
		// cout << "\t\tgetTagFromServer:: Promise kept by " << client_id << endl;
	} catch (const std::future_error& e) {
		// cout << "\t\tgetTagFromServer:: Promise is not longer needed from " << client_id << endl;
	}
}


uint32_t getHighestTagFromMajorityAsync(vector<KVClient*> &channels, const struct Client *c) {
	int n_channels = (int)channels.size();

	// create clients async
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

	// get clients from future
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

	// cout << "\tgetHighestTagFromMajorityAsync:: got tags from majority, dicarding other tags\n";

	while (n_got < n_channels) {
		std::future<uint32_t> fu = std::move(tag_futures.front()); tag_futures.pop();
		std::promise<uint32_t> pr = std::move(tag_promises.front()); tag_promises.pop();
		if (!is_ready(fu)) {
			// cout << "\tgetHighestTagFromMajorityAsync:: set exception to this promise\n";
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		} else {
			// cout << "\tgetHighestTagFromMajorityAsync:: promise fulfilled, but not needed\n";
		}
		fu.get();
		n_got++;
	}

	return tag;
}


void writeToServer(KVClient *channel, const string &key, const string &value, 
			const uint32_t client_id, const uint32_t timestamp, promise<void> &write_promise) {
	int status = channel->write(key, value, client_id, timestamp);

	if(status == 0){ // Success
		cout << "writeToServer:: write succussful for client " << client_id << " , key " << key << endl;
		write_promise.set_value();
	}
	else {
		cerr << "writeToServer:: write failed with client " << client_id << " and key " << key << endl;
		write_promise.set_value();
	}

	return;
}


bool writeToServersAsync(vector<KVClient*> &channels, const string &key, const string &value, 
			const uint32_t client_id, const uint32_t timestamp) {
	int n_channels = (int)channels.size();

	queue<std::promise<void>> promises;
	queue<std::future<void>> futures;
	for (int i = 0; i<n_channels; i++) {
		std::promise<void> pr;
		std::future<void> fu = pr.get_future();
		std::async(std::launch::async, writeToServer, channels[i], key, value, client_id, timestamp, ref(pr));
		promises.push(std::move(pr));
		futures.push(std::move(fu));
	}

	// get clients from future
	int n_majority = n_channels / 2 + 1;
	int n_got = 0;
	while (n_got < n_majority) {
		std::future<void> fu = std::move(futures.front()); futures.pop();
		std::promise<void> pr = std::move(promises.front()); promises.pop();
		if (is_ready(fu)) {
			fu.get();
			n_got++;
		}
		else {
			futures.push(std::move(fu));
			promises.push(std::move(pr));
		}
	}

	cout << "writeToServersAsync:: client " << client_id << ":: got ack from majority, dicarding other ack\n";

	while (n_got < n_channels) {
		std::future<void> fu = std::move(futures.front()); futures.pop();
		std::promise<void> pr = std::move(promises.front()); promises.pop();
		if (!is_ready(fu)) {
			cout << "\twriteToServersAsync:: client " << client_id << ":: set exception to this promise\n";
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		} else {
			cout << "\twriteToServersAsync:: client " << client_id << ":: promise fulfilled, but not needed\n";
		}
		fu.get();
		n_got++;
	}

	return true;
}


void readFromServer(KVClient *channel, const string &key, const uint32_t client_id, 
			promise<valueWithTag> &read_promise) {
	valueWithTag reply = channel->read(key, client_id);

	if(reply.value.compare("RPC failed") != 0){ // Success
		cout << "readFromServer:: read succussful for client " << client_id << " , key " << key << endl;
		read_promise.set_value(reply);
	}
	else {
		cerr << "readFromServer:: read failed with client " << client_id << " and key " << key << endl;
	}

	return;
}


string readFromServersAsync(vector<KVClient*> &channels, const string &key, const uint32_t client_id) {
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

	// get clients from future
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

	cout << "readFromServersAsync:: client " << client_id << ":: got ack from majority, dicarding other ack\n";

	while (n_got < n_channels) {
		std::future<valueWithTag> fu = std::move(futures.front()); futures.pop();
		std::promise<valueWithTag> pr = std::move(promises.front()); promises.pop();
		if (!is_ready(fu)) {
			cout << "\treadFromServersAsync:: client " << client_id << ":: set exception to this promise\n";
			pr.set_exception(std::make_exception_ptr(std::runtime_error("No need")));
		} else {
			cout << "\treadFromServersAsync:: client " << client_id << ":: promise fulfilled, but not needed\n";
		}
		fu.get();
		n_got++;
	}

	return value;
}


struct Client* client_instance(const uint32_t id, const char *protocol, const struct Server_info *servers, uint32_t number_of_servers)
{
	Client *client = new Client();
	client->id = id;
	if (strcmp(protocol,"ABD") == 0)
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
	cout << "client_instance:: initialized " << client->protocol << " client\n";
	return client;
}


int put(const struct Client *c, const char *key, uint32_t key_size, const char *value, uint32_t value_size)
{
	cout << "put:: client " << c->id << endl;

	string _key = string(key);
	string _value = string(value);
	_value.pop_back(); // avoid the extra newline character

	vector<KVClient*> channels = getKVClientsAsync(c);

	int highestTag = getHighestTagFromMajorityAsync(channels, c);

	cout << "put:: client " << c->id << " highest tag acquired from majority " << highestTag << endl;

	writeToServersAsync(channels, _key, _value, c->id, highestTag + 1);

	cout << "put:: client " << c->id << " complete" << endl;

	return 0;
}


int get(const struct Client *c, const char *key, uint32_t key_size, char **value, uint32_t *value_size)
{
	cout << "get:: client " << c->id << endl;

	string _key = string(key);

	vector<KVClient*> channels = getKVClientsAsync(c);

	string valueStr = readFromServersAsync(channels, _key, c->id);

	cout << valueStr << endl;

	// *value_size = valueStr.length();
	// valueStr.copy(*value, *value_size);

	return 0;
}


int client_delete(struct Client *c)
{
	delete c;
	return 0;
}


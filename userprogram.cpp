//
// Created for CSE513 course, Project 1
// EECS Department
// Pennsylvania State University
//

/* This program will create 3 clients and do a read and a write operation with each of them cuncurrently.
 * Please set the varaiable servers before use
 */

#include "client.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <thread>
#include <memory>

clock_t  clock_init;

typedef unsigned int uint;

#define NUMBER_OF_CLIENTS 	10
#define NUM_READS 			5
#define SIZE_OF_VALUE 		1024

std::string TYPES[2] = {"invoke", "ok"};
std::string OPS[2] = {"write", "read"};
std::string NIL = "nil";
static struct Server_info servers[] = {
				       {"127.0.0.1", 10000},
		{"127.0.0.1", 10001},
		{"127.0.0.1", 10002},
		{"127.0.0.1", 10003},
		{"127.0.0.1", 10004},
		{"127.0.0.1", 10005},
		{"127.0.0.1", 10006},
		{"127.0.0.1", 10007},
		{"127.0.0.1", 10008},
		{"127.0.0.1", 10009}};

static char key[] = "123456"; // We only have one key in this userprogram


std::string log_string(int client_id, std::string type, std::string op, const std::string &value) {
	return "{:process " + std::to_string(client_id) + ", " 
			+ ":type :" + type + ", "
			+ ":f :" + op + ", "
			+ ":value " + value + "}\n";
}

namespace Thread_helper{
	void _put(const struct Client* c, const char* key, uint32_t key_size, 
				const char* value, uint32_t value_size, std::ofstream& out){
		std::string _value = std::string(value);
		_value.pop_back();

		clock_t  clock_curr = clock();
		float time_elapsed = (float)(clock_curr - clock_init)/ CLOCKS_PER_SEC;
		out << time_elapsed << "\t" << log_string(c->id, TYPES[0], OPS[0], _value);

		int status = put(c, key, key_size, value, value_size);

		clock_curr = clock();
		time_elapsed = (float)(clock_curr - clock_init)/ CLOCKS_PER_SEC;
		out << time_elapsed << "\t" << log_string(c->id, TYPES[1], OPS[0], _value);

		if(status == 0){ // Success
			return;
		}
		else{
			exit(-1);
		}

		return;
	}

	void _get(const struct Client* c, const char* key, uint32_t key_size, 
				char** value, uint32_t *value_size, std::ofstream& out){

		clock_t  clock_curr = clock();
		float time_elapsed = (float)(clock_curr - clock_init)/ CLOCKS_PER_SEC;
		out << time_elapsed << "\t" << log_string(c->id, TYPES[0], OPS[1], NIL);

		int status = get(c, key, key_size, value, value_size);

		std::string valueStr = *value;

		clock_curr = clock();
		time_elapsed = (float)(clock_curr - clock_init)/ CLOCKS_PER_SEC;
		out << time_elapsed << "\t" << log_string(c->id, TYPES[1], OPS[1], valueStr);

		if(status == 0){ // Success
			return;
		}
		else{
			exit(-1);
		}

		return;
	}

	void _request10(const struct Client* c, const char* key, uint32_t key_size, bool do_read, std::ofstream& out) {
		for (int i=0; i<10; i++) {
			clock_t  clock_init = clock();

			srand(time(NULL));

			if (do_read) {
				char* value;
				uint32_t value_size;
				_get(c, key, key_size, &value, &value_size, out);

				// handle memory leak
				delete value;

				// std::cout << "client " << c->id << " : <get> done\n";
			}
			else {
				char value[SIZE_OF_VALUE];
				for(int i = 0; i < SIZE_OF_VALUE; i++){
					value[i] = '0' + rand() % 10;
				}

				_put(c, key, key_size, value, sizeof(value), out);

				// std::cout << "client " << c->id << " : <put> done\n";
			}

			clock_t clock_curr = clock();
			float time_elapsed = (float)(clock_curr - clock_init)/ ( CLOCKS_PER_SEC / 1000 );

			if (time_elapsed < 1000) {
				uint32_t timetosleep = 1000 - time_elapsed;
				std::this_thread::sleep_for(std::chrono::milliseconds(timetosleep));
			}
		}
	}
}

int main(int argc, char* argv[]){

	clock_init = clock();

	if(argc != 2){
		fprintf(stderr, "%s%s%s\n", "Error\n"
		"Usage: ", argv[0], "[ABD/CM]\n\n"
		"Please note to run the servers first\n"
		"This application is just for your testing purposes, "
		"and the evaluation of your code will be done with another initiation of your Client libraries.");
		return -1;
	}

	if(std::string(argv[1]) == "ABD"){
		std::vector<std::ofstream> streams;

		// Create ABD clients
		struct Client* abd_clt[NUMBER_OF_CLIENTS];
		for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
			abd_clt[i] = client_instance(i, "ABD", servers, sizeof(servers) / sizeof(struct Server_info));
			if(abd_clt[i] == NULL){
				fprintf(stderr, "%s\n", "Error occured in creating clients");
				return -1;
			}

			// create a log file
			std::string logfile = "logs/local/clientlogs_" + std::to_string(i) + ".txt";
			std::ofstream stream_i(logfile);
			streams.push_back(std::move(stream_i));
		}

		// Do write operations concurrently
		std::vector<std::thread*> threads;

		// Do get operations concurrently
		threads.clear();
		for(uint i = 0; i < NUM_READS; i++){
			// run the thread
			threads.push_back(new std::thread(Thread_helper::_request10, abd_clt[i], key, sizeof(key), true, std::ref(streams[i])));
	    }
		for(uint i = NUM_READS; i < NUMBER_OF_CLIENTS; i++){
			// run the thread
			threads.push_back(new std::thread(Thread_helper::_request10, abd_clt[i], key, sizeof(key), false, std::ref(streams[i])));
	    }
	    // Wait for all threads to join
	    for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
	    	threads[i]->join();
	    }

		// Clean up allocated memory in struct Client
		for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
			if(client_delete(abd_clt[i]) == -1){
				fprintf(stderr, "%s\n", "Error occured in deleting clients");
				return -1;
			}
		}
	}
	else if(std::string(argv[1]) == "CM"){
		
		std::vector<std::ofstream> streams;

		// Create CM clients
		struct Client* cm_clt[NUMBER_OF_CLIENTS];
		for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
			cm_clt[i] = client_instance(i, "CM", servers, sizeof(servers) / sizeof(struct Server_info));
			if(cm_clt[i] == NULL){
				fprintf(stderr, "%s\n", "Error occured in creating clients");
				return -1;
			}

			// create a log file
			std::string logfile = "logs/local_cm/clientlogs_" + std::to_string(i) + ".txt";
			std::ofstream stream_i(logfile);
			streams.push_back(std::move(stream_i));
		}

		// Do write operations concurrently
		std::vector<std::thread*> threads;

		threads.clear();
		for(uint i = 0; i < NUM_READS; i++){
			// run the thread
			threads.push_back(new std::thread(Thread_helper::_request10, cm_clt[i], key, sizeof(key), true, std::ref(streams[i])));
	    }
		for(uint i = NUM_READS; i < NUMBER_OF_CLIENTS; i++){
			// run the thread
			threads.push_back(new std::thread(Thread_helper::_request10, cm_clt[i], key, sizeof(key), false, std::ref(streams[i])));
	    }
	    // Wait for all threads to join
	    for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
	    	threads[i]->join();
	    }

		// Clean up allocated memory in struct Client
		for(uint i = 0; i < NUMBER_OF_CLIENTS; i++){
			if(client_delete(cm_clt[i]) == -1){
				fprintf(stderr, "%s\n", "Error occured in deleting clients");
				return -1;
			}
		}
	}
	else{
		fprintf(stderr, "%s%s%s\n", "Error\n"
		"Usage: ", argv[0], "[ABD/CM]\n\n"
		"Please note to run the servers first\n"
		"This application is just for your testing purposes, "
		"and the evaluation of your code will be done with another initiation of your Client libraries.");
		return -1;
	}

	return 0;
}

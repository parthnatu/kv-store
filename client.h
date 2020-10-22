//
// Created for CSE513 course, Project 1
// EECS Department
// Pennsylvania State University
//

#ifndef PROJECT1_CLIENT_H
#define PROJECT1_CLIENT_H

#include <stdint.h>
#include <cstddef>
//#include <iostream>
//using namespace std;
struct Server_info{
    char ip[16]; // IP Sample: "192.168.1.2"
    uint16_t port;
};


struct Client{
    uint32_t id;
    char protocol[4]; // ABD or CM
    const struct Server_info* servers; // An array containing the infromation to access each server
    uint32_t number_of_servers; // Number of elements in the array servers
};


/* This function will instantiate a client and initialize all the necessary variables.
 * id is the id of the client. class_name can be "ABD" or "CM" indicating the type of the client.
 * servers is an array of attributes of each server. number_of_servers determine the number of elements in the array.
 *
 * Returns a pointer to the created client. It returns NULL if an error occurs.
 */
struct Client* client_instance(const uint32_t id, const char* protocol, const struct Server_info* servers, uint32_t number_of_servers);


/* This function will write the value specified in the variable value into the key specified by the variable key.
 * the number of characters expected in key and value are determined by key_size and value_size respectfully.
 *
 * Returns 0 on success, and -1 on error.
 */
int put(const struct Client* c, const char* key, uint32_t key_size, const char* value, uint32_t value_size);


/* This function will read the value of the key specified by the variable key.
 * the number of characters expected in key is determined by key_size. Also the read value will be written to a portion
 * of memory and the address the first element of that portion will be written to the memory pointed by the variable
 * value. And, the size of the read value will be written to the memory pointed by value_size.
 *
 * Returns 0 on success, and -1 on error.
 */
int get(const struct Client* c, const char* key, uint32_t key_size, char** value, uint32_t *value_size);


/* This function will destroy all the memory that might have been allocated and needs to be cleaned up.
 *
 * Returns 0 on success, and -1 on error.
 */
int client_delete(struct Client* c);

#endif //PROJECT1_CLIENT_H

struct Client* client_instance(const uint32_t id, const char* protocol, const struct Server_info* servers, uint32_t number_of_servers){
  Client* client = new Client();
  client->id = id;
  if(protocol == "ABD"){
    for(int i=0;i<3;i++)
      client->protocol[i] = protocol[i];
    client->protocol[3] = '\0';
  }
  else{
    for(int i=0;i<2;i++)
      client->protocol[i] = protocol[i];
    client->protocol[2] = '\0';
  }
  client->servers = servers;
  client->number_of_servers = number_of_servers;
  //  cout << "initialized " << client->protocol << " client\n";
  return client;
}

int put(const struct Client* c, const char* key, uint32_t key_size, const char* value, uint32_t value_size){
  return 0;
}

int get(const struct Client* c, const char* key, uint32_t key_size, char** value, uint32_t *value_size){
  return 0;
}

int client_delete(struct Client* c){
  delete c;
  return 0;
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#define MAX_CLIENTS 1005
// #define DEBUG 1

#ifndef _HEADER_HPP_
#define _HEADER_HPP_
struct User {
    std::string username;
    std::string password;
    std::string status = "offline";  // default status is offline
};

struct ChatRecord {
    std::string username;
    std::string message;
};

class ChatRoom {
   public:
    std::string owner;
    std::vector<int> clients;
    std::vector<ChatRecord> history;
    std::pair<std::string, std::string> pinned_message;  // username and message
    std::vector<std::string> filter_list;
    ChatRoom();
    ChatRoom(const std::string &owner);
};

class Server {
   private:
    std::map<int, User *> clients;
    std::vector<User *> registerd_user;
    std::map<int, ChatRoom> chatRooms;
    std::set<std::string> loggedInUsers;
    std::map<int, int> client_in_chat_room;
    fd_set master_set;

   public:
    Server();
    fd_set getMasterSet();
    void addSocket(int socket);
    void removeSocket(int socket);
    std::string registerUser(std::vector<std::string> tokens);
    std::string loginUser(std::vector<std::string> tokens, int client_socket);
    std::string logoutUser(std::vector<std::string> tokens, int client_socket);
    std::string exitUser(std::vector<std::string> tokens, int client_socket, int client_socket_list[]);
    std::string getwhoami(std::vector<std::string> tokens, int client_socket);
    std::string setClientStatus(std::vector<std::string> tokens, int client_socket);
    std::string listUser(std::vector<std::string> tokens, int client_socket);
    std::string enterChatRoom(std::vector<std::string> tokens, int client_socket);
    std::string listChatRoom(std::vector<std::string> tokens, int client_socket);
    std::string closeChatRoom(std::vector<std::string> tokens, int client_socket);
    std::string setPinMessage(std::vector<std::string> tokens, int client_socket);
    std::string deletePinMessage(std::vector<std::string> tokens, int client_socket);
    std::string exitChatRoom(std::vector<std::string> tokens, int client_socket);
    std::string listUserinChatRoom(std::vector<std::string> tokens, int client_socket);
    std::string sendMessage(std::vector<std::string> tokens, int client_socket);
    void handleCommand(int client_socket, std::string command, int client_socket_list[]);
};

int setupServer(int port);

#endif
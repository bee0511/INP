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

#define MAX_CLIENTS 30

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
    std::string pinned_message;
    ChatRoom() = default;
    ChatRoom(const std::string &owner) : owner(owner) {
        clients.clear();
        history.clear();
        pinned_message = "";
    }
};

class Server {
   private:
    std::map<int, User *> clients;
    std::vector<User *> registerd_user;
    std::map<int, ChatRoom> chatRooms;
    std::set<std::string> loggedInUsers;
    std::map<int, bool> is_client_in_chat_room;
    fd_set master_set;

   public:
    Server() {
        FD_ZERO(&master_set);
        clients.clear();
        registerd_user.clear();
        chatRooms.clear();
        loggedInUsers.clear();
        is_client_in_chat_room.clear();
    }

    fd_set getMasterSet() {
        return master_set;
    }

    void addSocket(int socket) {
        FD_SET(socket, &master_set);
    }

    void removeSocket(int socket) {
        FD_CLR(socket, &master_set);
    }

    std::string registerUser(std::vector<std::string> tokens) {
        if (tokens.size() != 3) {
            return "Usage: register <username> <password>\n";
        }
        std::string username = tokens[1];
        std::string password = tokens[2];
        for (auto user : registerd_user) {
            if (user->username == username) {
                return "Username is already used.\n";
            }
        }
        User *user = new User();
        user->username = username;
        user->password = password;
        registerd_user.push_back(user);
        return "Register successfully.\n";
    }

    std::string loginUser(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 3) {
            return "Usage: login <username> <password>\n";
        }
        std::string username = tokens[1];
        std::string password = tokens[2];
        // check if the client is already logged in
        if (clients.find(client_socket) != clients.end() && loggedInUsers.find(clients[client_socket]->username) != loggedInUsers.end()) {
            return "Please logout first.\n";
        }
        // check if the user is already logged in
        if (loggedInUsers.find(username) != loggedInUsers.end()) {
            return "Login failed.\n";
        }
        for (auto user : registerd_user) {
            if (user->username == username && user->password == password && loggedInUsers.find(username) == loggedInUsers.end()) {
                clients[client_socket] = user;
                clients[client_socket]->status = "online";  // set the status to online after login
                loggedInUsers.insert(username);             // add the username to the set of logged-in registerd_user
                return "Welcome, " + username + ".\n";
            }
        }
        return "Login failed.\n";
    }

    std::string logoutUser(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 1) {
            return "Usage: logout\n";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        std::string response = "Bye, " + clients[client_socket]->username + ".\n";
        loggedInUsers.erase(clients[client_socket]->username);  // remove the username from the set of logged-in registerd_user
        clients[client_socket]->status = "offline";             // set the status to offline after logout
        clients.erase(client_socket);
        return response;
    }

    std::string exitUser(std::vector<std::string> tokens, int client_socket, int client_socket_list[]) {
        if (tokens.size() != 1) {
            return "Usage: exit\n";
        }
        std::string response;
        if (clients.find(client_socket) != clients.end() && loggedInUsers.find(clients[client_socket]->username) != loggedInUsers.end()) {
            response = logoutUser(tokens, client_socket);
            send(client_socket, response.c_str(), response.size(), 0);
        }
        removeSocket(client_socket);
        close(client_socket);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_socket_list[i] == client_socket) {
                client_socket_list[i] = 0;
                break;
            }
        }
        return response;
    }

    std::string getwhoami(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 1) {
            return "Usage: whoami\n";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        return clients[client_socket]->username + "\n";
    }

    std::string setClientStatus(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 2) {
            return "Usage: set-status <status>\n";
        }
        std::string status = tokens[1];
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        if (status != "online" && status != "offline" && status != "busy") {
            return "set-status failed\n";
        }
        clients[client_socket]->status = status;
        return clients[client_socket]->username + " " + status + "\n";
    }

    std::string listUser(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 1) {
            return "Usage: list-user\n";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        // sort the registerd_user by username
        std::sort(registerd_user.begin(), registerd_user.end(), [](User *a, User *b) { return a->username < b->username; });
        std::string response;
        for (auto user : registerd_user) {
            response += user->username + " " + user->status + "\n";
        }
        return response;
    }

    std::string enterChatRoom(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 2) {
            return "Usage: enter-chat-room <number>\n% ";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n% ";
        }
        // check the tokens[1] is a number between 1 and 100
        try {
            std::stoi(tokens[1]);
        } catch (const std::invalid_argument &ia) {
            return "Number " + tokens[1] + " is not valid.\n% ";
        }
        int room_number = std::stoi(tokens[1]);
        if (room_number < 1 || room_number > 100) {
            return "Number " + tokens[1] + " is not valid.\n% ";
        }
        if (chatRooms.find(room_number) == chatRooms.end()) {
            // Create a new room
            chatRooms[room_number] = ChatRoom(clients[client_socket]->username);
            chatRooms[room_number].clients.push_back(client_socket);
            is_client_in_chat_room[client_socket] = true;
            return "Welcome to the public chat room.\nRoom number: " + std::to_string(room_number) + "\nOwner: " + clients[client_socket]->username + "\n";
        }
        std::string response;
        // Enter the existing room
        chatRooms[room_number].clients.push_back(client_socket);
        response = "Welcome to the public chat room.\nRoom number: " + std::to_string(room_number) + "\nOwner: " + chatRooms[room_number].owner + "\n";
        is_client_in_chat_room[client_socket] = true;

        // Show the latest 10 records
        int start = std::max(0, (int)chatRooms[room_number].history.size() - 10);
        for (int i = start; i < (int)chatRooms[room_number].history.size(); i++) {
            response += "[" + chatRooms[room_number].history[i].username + "]: " + chatRooms[room_number].history[i].message + "\n";
        }

        // Broadcast the message to all clients in the room
        std::string broadcast = clients[client_socket]->username + " had enter the chat room.\n";
        for (int client : chatRooms[room_number].clients) {
            if (client == client_socket) {
                continue;
            }
            send(client, broadcast.c_str(), broadcast.size(), 0);
        }
        return response;
    }

    std::string listChatRoom(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 1) {
            return "Usage: list-chat-room\n";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        std::string response;

        // List all existing chat rooms and the corresponding owners in the server and sort by the room number.
        std::vector<int> room_numbers;
        for (auto room : chatRooms) {
            room_numbers.push_back(room.first);
        }
        std::sort(room_numbers.begin(), room_numbers.end());
        for (auto room_number : room_numbers) {
            response += "Room " + std::to_string(room_number) + " " + chatRooms[room_number].owner + "\n";
        }
        return response;
    }

    std::string closeChatRoom(std::vector<std::string> tokens, int client_socket) {
        if (tokens.size() != 2) {
            return "Usage: close-chat-room <number>\n";
        }
        if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
            return "Please login first.\n";
        }
        // if the chat room does not exist, return "Chat room <number> does not exist.\n"
        try {
            std::stoi(tokens[1]);
        } catch (const std::invalid_argument &ia) {
            return "Chat room " + tokens[1] + " does not exist.\n";
        }
        int room_number = std::stoi(tokens[1]);
        if (chatRooms.find(room_number) == chatRooms.end()) {
            return "Chat room " + tokens[1] + " does not exist.\n";
        }
        // if the user is not the owner of the chat room, return "Only the owner can close this chat room.\n"
        if (chatRooms[room_number].owner != clients[client_socket]->username) {
            return "Only the owner can close this chat room.\n";
        }
        std::string response = "Chat room " + tokens[1] + " was closed.\n";
        // Broadcast the message to all clients in the room
        std::string broadcast = "Chat room " + tokens[1] + " was closed.\n% ";
        for (int client : chatRooms[room_number].clients) {
            is_client_in_chat_room[client] = false;
            if (client == client_socket) {
                continue;
            }
            send(client, broadcast.c_str(), broadcast.size(), 0);
        }
        chatRooms.erase(room_number);
        return response;
    }

    void handleCommand(int client_socket, std::string command, int client_socket_list[]) {
        std::istringstream iss(command);
        std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());

        if (tokens.empty() && is_client_in_chat_room[client_socket] == false) {
            std::string prompt = "% ";
            send(client_socket, prompt.c_str(), prompt.size(), 0);
            return;
        }

        if (tokens.empty() && is_client_in_chat_room[client_socket] == true) {
            std::string prompt = "";
            send(client_socket, prompt.c_str(), prompt.size(), 0);
            return;
        }

        std::string response;
        if (tokens[0] == "register") {
            response = registerUser(tokens);
            response += "% ";
        } else if (tokens[0] == "login") {
            response = loginUser(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "logout") {
            response = logoutUser(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "exit") {
            response = exitUser(tokens, client_socket, client_socket_list);
            response += "% ";
        } else if (tokens[0] == "whoami") {
            response = getwhoami(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "set-status") {
            response = setClientStatus(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "list-user") {
            response = listUser(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "enter-chat-room") {
            response = enterChatRoom(tokens, client_socket);
        } else if (tokens[0] == "list-chat-room") {
            response = listChatRoom(tokens, client_socket);
            response += "% ";
        } else if (tokens[0] == "close-chat-room") {
            response = closeChatRoom(tokens, client_socket);
            response += "% ";
        } else {
            response = "Error: Unknown command\n";
        }

        send(client_socket, response.c_str(), response.size(), 0);
    }
};

int setupServer(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the specified port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Forcefully attaching socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Listening on port " << port << std::endl;

    return server_fd;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./hw2_chat_server [port number]" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    int server_fd = setupServer(port);

    int client_socket[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_socket[i] = 0;
    }

    int activity, new_socket, sd, max_sd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    Server server;

    while (true) {
        // Set of socket descriptors
        fd_set readfds = server.getMasterSet();

        // Add master socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            // Socket descriptor
            sd = client_socket[i];

            // If valid socket descriptor then add to read list
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }

            // Highest file descriptor number, need it for the select function
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        // If something happened on the master socket, then its an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // Inform user of socket number - used in send and receive commands
            std::cout << "New connection, socket fd is " << new_socket << ", ip is : " << inet_ntoa(address.sin_addr) << ", port : " << ntohs(address.sin_port) << std::endl;

            // Send prompt to the new client
            std::string prompt = "";
            prompt += "*********************************\n";
            prompt += "** Welcome to the Chat server. **\n";
            prompt += "*********************************\n";
            prompt += "% ";
            send(new_socket, prompt.c_str(), prompt.size(), 0);

            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                // If position is empty
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    server.addSocket(new_socket);  // add the new socket to the master set
                    std::cout << "Adding to list of sockets as " << i << std::endl;
                    break;
                }
            }
        }

        // Else its some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (sd == 0) {
                continue;
            }
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                char buffer[1024] = {0};
                int valread = read(sd, buffer, 1024);
                if (valread == 0) {
                    // Client closed connection
                    close(sd);
                    client_socket[i] = 0;
                    server.removeSocket(sd);  // remove the socket from the master set
                } else if (valread < 0) {
                    // Read error
                    perror("read");
                } else {
                    std::string message = std::string(buffer, valread);
                    message.pop_back();

                    std::cout << "Client " << sd << " sent: " << message << std::endl;

                    // Handle the command
                    server.handleCommand(sd, message, client_socket);
                }
            }
        }
    }

    return 0;
}
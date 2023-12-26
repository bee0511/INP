#include "header.hpp"

Server::Server() {
    FD_ZERO(&master_set);
    clients.clear();
    registerd_user.clear();
    chatRooms.clear();
    loggedInUsers.clear();
    client_in_chat_room.clear();
}

fd_set Server::getMasterSet() {
    return master_set;
}

void Server::addSocket(int socket) {
    FD_SET(socket, &master_set);
}

void Server::removeSocket(int socket) {
    FD_CLR(socket, &master_set);
}

std::string Server::registerUser(std::vector<std::string> tokens) {
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

std::string Server::loginUser(std::vector<std::string> tokens, int client_socket) {
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

std::string Server::logoutUser(std::vector<std::string> tokens, int client_socket) {
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

std::string Server::exitUser(std::vector<std::string> tokens, int client_socket, int client_socket_list[]) {
    if (tokens.size() != 1) {
        return "Usage: exit\n";
    }
    std::string response;
    // check if the client is already logged in
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

std::string Server::getwhoami(std::vector<std::string> tokens, int client_socket) {
    if (tokens.size() != 1) {
        return "Usage: whoami\n";
    }
    if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
        return "Please login first.\n";
    }
    return clients[client_socket]->username + "\n";
}

std::string Server::setClientStatus(std::vector<std::string> tokens, int client_socket) {
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

std::string Server::listUser(std::vector<std::string> tokens, int client_socket) {
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

std::string Server::enterChatRoom(std::vector<std::string> tokens, int client_socket) {
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
        client_in_chat_room[client_socket] = room_number;
        return "Welcome to the public chat room.\nRoom number: " + std::to_string(room_number) + "\nOwner: " + clients[client_socket]->username + "\n";
    }
    std::string response;
    // Enter the existing room
    chatRooms[room_number].clients.push_back(client_socket);
    response = "Welcome to the public chat room.\nRoom number: " + std::to_string(room_number) + "\nOwner: " + chatRooms[room_number].owner + "\n";
    client_in_chat_room[client_socket] = room_number;

    // Show the latest 10 records
    int start = std::max(0, (int)chatRooms[room_number].history.size() - 10);
    for (int i = start; i < (int)chatRooms[room_number].history.size(); i++) {
        response += "[" + chatRooms[room_number].history[i].username + "]: " + chatRooms[room_number].history[i].message + "\n";
    }

    // Show the pinned message
    if (chatRooms[room_number].pinned_message.first != "") {
        response += "Pin -> [" + chatRooms[room_number].pinned_message.first + "]: " + chatRooms[room_number].pinned_message.second + "\n";
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

std::string Server::listChatRoom(std::vector<std::string> tokens, int client_socket) {
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
        response += chatRooms[room_number].owner + " " + std::to_string(room_number) + "\n";
    }
    return response;
}

std::string Server::closeChatRoom(std::vector<std::string> tokens, int client_socket) {
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
        client_in_chat_room.erase(client);
        if (client == client_socket) {
            continue;
        }
        send(client, broadcast.c_str(), broadcast.size(), 0);
    }
    chatRooms.erase(room_number);
    return response;
}

std::string Server::setPinMessage(std::vector<std::string> tokens, int client_socket) {
    if (tokens.size() < 2) {
        return "Usage: /pin <message>\n";
    }
    if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
        return "Please login first.\n";
    }
    if (client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        return "You are not in the chat room.\n";
    }
    int chat_room_number = client_in_chat_room[client_socket];
    std::string message = "";
    for (int i = 1; i < (int)tokens.size(); i++) {
        message += tokens[i] + " ";
    }
    message.pop_back();

    // check if the message contains any word in the filter list
    // the filter list is case insensitive
    // if the message contain the word in the filter list, replace the word with the * character
    std::string message_lower = message;
    std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);
    for (auto word : chatRooms[chat_room_number].filter_list) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        size_t pos = message_lower.find(word);
        while (pos != std::string::npos) {
            message.replace(pos, word.size(), std::string(word.size(), '*'));
            message_lower.replace(pos, word.size(), std::string(word.size(), '*'));
            pos = message_lower.find(word);
        }
    }

    // add the pin message to the chat room
    chatRooms[chat_room_number].pinned_message = std::make_pair(clients[client_socket]->username, message);

    // send the pin message to all clients
    std::string broadcast = "Pin -> [" + clients[client_socket]->username + "]: " + message + "\n";
    for (int client : chatRooms[chat_room_number].clients) {
        if (client == client_socket) {
            continue;
        }
        send(client, broadcast.c_str(), broadcast.size(), 0);
    }
    return "Pin -> [" + clients[client_socket]->username + "]: " + message + "\n";
}

std::string Server::deletePinMessage(std::vector<std::string> tokens, int client_socket) {
    if (tokens.size() != 1) {
        return "Usage: /delete-pin\n";
    }
    if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
        return "Please login first.\n";
    }
    if (client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        return "You are not in the chat room.\n";
    }
    // If there is no pin message in the chat room -> print No pin messages in chat room <number>
    if (chatRooms[client_in_chat_room[client_socket]].pinned_message.first == "") {
        return "No pin message in chat room " + std::to_string(client_in_chat_room[client_socket]) + "\n";
    }
    // delete the pin message in the chat room
    chatRooms[client_in_chat_room[client_socket]].pinned_message = std::make_pair("", "");
    return "";
}

std::string Server::exitChatRoom(std::vector<std::string> tokens, int client_socket) {
    if (tokens.size() != 1) {
        return "Usage: /exit-chat-room\n";
    }
    if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
        return "Please login first.\n";
    }
    if (client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        return "You are not in the chat room.\n";
    }
    // remove the client from the chat room
    int room_number = client_in_chat_room[client_socket];
    for (int i = 0; i < (int)chatRooms[room_number].clients.size(); i++) {
        if (chatRooms[room_number].clients[i] == client_socket) {
            chatRooms[room_number].clients.erase(chatRooms[room_number].clients.begin() + i);
            break;
        }
    }
    // Send the message <username> had left the chat room. to all clients in the chat room.
    std::string broadcast = clients[client_socket]->username + " had left the chat room.\n";
    for (int client : chatRooms[room_number].clients) {
        if (client == client_socket) {
            continue;
        }
        send(client, broadcast.c_str(), broadcast.size(), 0);
    }
    client_in_chat_room.erase(client_socket);
    return "% ";  // turn back to chat server mode
}

std::string Server::listUserinChatRoom(std::vector<std::string> tokens, int client_socket) {
    if (tokens.size() != 1) {
        return "Usage: /list-user\n";
    }
    if (clients.find(client_socket) == clients.end() || loggedInUsers.find(clients[client_socket]->username) == loggedInUsers.end()) {
        return "Please login first.\n% ";
    }
    if (client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        return "You are not in the chat room.\n% ";
    }
    // print out the user and its status in alphabetical order
    std::string response;
    std::vector<User *> users;
    for (int client : chatRooms[client_in_chat_room[client_socket]].clients) {
        users.push_back(clients[client]);
    }
    std::sort(users.begin(), users.end(), [](User *a, User *b) {
        return a->username < b->username;
    });
    for (auto user : users) {
        response += user->username + " " + user->status + "\n";
    }
    return response;
}

std::string Server::sendMessage(std::vector<std::string> tokens, int client_socket) {
    if (client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        return "You are not in the chat room.\n";
    }
    std::string message = "";
    for (int i = 0; i < (int)tokens.size(); i++) {
        message += tokens[i] + " ";
    }
    message.pop_back();
    // check if the message contains any word in the filter list
    // the filter list is case insensitive
    // if the message contain the word in the filter list, replace the word with the * character
    std::string message_lower = message;
    std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);
    for (auto word : chatRooms[client_in_chat_room[client_socket]].filter_list) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        size_t pos = message_lower.find(word);
        while (pos != std::string::npos) {
            message.replace(pos, word.size(), std::string(word.size(), '*'));
            message_lower.replace(pos, word.size(), std::string(word.size(), '*'));
            pos = message_lower.find(word);
        }
    }
    // add the message to the chat room history
    ChatRecord record;
    record.username = clients[client_socket]->username;
    record.message = message;
    chatRooms[client_in_chat_room[client_socket]].history.push_back(record);
    // send the message to all clients in the room
    std::string broadcast = "[" + clients[client_socket]->username + "]: " + message + "\n";
    for (int client : chatRooms[client_in_chat_room[client_socket]].clients) {
        if (client == client_socket) {
            continue;
        }
        send(client, broadcast.c_str(), broadcast.size(), 0);
    }
    return broadcast;
}

void Server::handleCommand(int client_socket, std::string command, int client_socket_list[]) {
    std::istringstream iss(command);
    std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());

    if (tokens.empty() && client_in_chat_room.find(client_socket) == client_in_chat_room.end()) {
        std::string prompt = "% ";
        send(client_socket, prompt.c_str(), prompt.size(), 0);
        return;
    }

    if (tokens.empty() && client_in_chat_room.find(client_socket) != client_in_chat_room.end()) {
        std::string prompt = "";
        send(client_socket, prompt.c_str(), prompt.size(), 0);
        return;
    }

    std::string response;

    // handle the command in the chat room
    if (client_in_chat_room.find(client_socket) != client_in_chat_room.end()) {
        if (tokens[0] == "/pin") {
            response = setPinMessage(tokens, client_socket);
        } else if (tokens[0] == "/delete-pin") {
            response = deletePinMessage(tokens, client_socket);
        } else if (tokens[0] == "/exit-chat-room") {
            response = exitChatRoom(tokens, client_socket);
        } else if (tokens[0] == "/list-user") {
            response = listUserinChatRoom(tokens, client_socket);
        } else if (tokens[0][0] == '/') {
            response = "Error: Unknown command\n";
        } else {
            // send chat message to all clients in the room
            response = sendMessage(tokens, client_socket);
        }
        send(client_socket, response.c_str(), response.size(), 0);
        return;
    }

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
        response = "Error: Unknown command\n% ";
    }

    send(client_socket, response.c_str(), response.size(), 0);
}

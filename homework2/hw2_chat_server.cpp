#include "header.hpp"

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
#ifdef DEBUG
    std::cout << "Listening on port " << port << std::endl;
#endif
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
#ifdef DEBUG
            // Inform user of socket number - used in send and receive commands
            std::cout << "New connection, socket fd is " << new_socket << ", ip is : " << inet_ntoa(address.sin_addr) << ", port : " << ntohs(address.sin_port) << std::endl;
#endif
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
#ifdef DEBUG
                    std::cout << "Adding to list of sockets as " << i << std::endl;
#endif
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

#ifdef DEBUG
                    std::cout << "Client " << sd << " sent: " << message << std::endl;
#endif
                    // Handle the command
                    server.handleCommand(sd, message, client_socket);
                }
            }
        }
    }

    return 0;
}
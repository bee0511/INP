#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>


using namespace std;
int main() {
    const char* server = "inp.zoolab.org";
    const char* port = "10314";
    const char* path = "/otp?name=110550164";  // Updated student ID

    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Resolve server's IP address
    struct hostent* server_info = gethostbyname(server);
    if (!server_info) {
        perror("Error resolving host");
        close(sock);
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(port));
    server_address.sin_addr.s_addr = *((unsigned long*)server_info->h_addr_list[0]);  // Use the first address in the list

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Construct the HTTP request
    std::string request = "GET " + std::string(path) + " HTTP/1.1\r\n" +
                         "Host: " + std::string(server) + "\r\n" +
                         "Connection: close\r\n" +
                         "\r\n";

    // Send the request
    if (send(sock, request.c_str(), request.size(), 0) == -1) {
        perror("Error sending request");
        close(sock);
        return 1;
    }

    // Receive and print the response
    char buffer[1024];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        std::cout.write(buffer, bytes_received);
    }
    
    string verify;
    for(int i = 0 ; i < 1024; i++){
        if(!(buffer[i] == '1' && buffer[i+1] == '1' && buffer[i+2] == '0' && buffer[i+3] == '5'))continue;
        int j = i;
        while(buffer[j] != '='){
            // cout << "j: " << j << endl;
            if(buffer[j] == '+'){
                verify.append("2b");
                j++;
                continue;
            }
            verify.push_back(buffer[j]);
            j++;
        }
        verify.append("==");
        break;
    }
    cout << endl << "Verify: " << verify << endl;

    

    if (bytes_received == -1) {
        perror("Error receiving response");
    }



    close(sock);
    return 0;
}

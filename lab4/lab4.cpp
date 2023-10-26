#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <fstream>

using namespace std;

int main() {
    const char* server = "inp.zoolab.org";
    const char* port = "10314";
    const char* path_otp = "/otp?name=110550164";  // Updated student ID
    const char* path_upload = "/upload";
    const char* output_file = "file_to_upload.txt"; // The file to upload

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
    server_address.sin_addr.s_addr = *((unsigned long*)server_info->h_addr_list[0]); // Use the first address in the list

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Construct and send the GET request to obtain the OTP
    std::string request_otp = "GET " + std::string(path_otp) + " HTTP/1.1\r\n" +
                              "Host: " + std::string(server) + "\r\n" +
                              "Connection: close\r\n" +
                              "\r\n";

    if (send(sock, request_otp.c_str(), request_otp.size(), 0) == -1) {
        perror("Error sending OTP request");
        close(sock);
        return 1;
    }

    // Receive and extract the OTP
    char buffer[1024];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        std::cout.write(buffer, bytes_received);
    }

    // Extract the OTP from the response using your provided code snippet
    string verify;
    for (int i = 0; i < 1024; i++) {
        if (!(buffer[i] == '1' && buffer[i + 1] == '1' && buffer[i + 2] == '0' && buffer[i + 3] == '5')) continue;
        int j = i;
        while (buffer[j] != '=') {
            if (buffer[j] == '+') {
                verify.append("+");
                j++;
                continue;
            }
            verify.push_back(buffer[j]);
            j++;
        }
        verify.append("==");
        break;
    }
    cout << endl << "Verify string: " << verify << endl;
    memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    close(sock);

    // Create a socket for the POST request
    int post_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (post_sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Connect to the server again
    if (connect(post_sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(post_sock);
        return 1;
    }

const char* boundary = "----WebKitFormBoundaryA6blcZTkTIB37MaT";

stringstream post_request;
post_request << "POST " << path_upload << " HTTP/1.1\r\n"
             << "Host: " << server << "\r\n"
             << "Connection: close\r\n"
             << "Content-Length: " << 298 << "\r\n"
             << "Cache-Control: max-age=0\r\n"
             << "Upgrade-Insecure-Requests: 1\r\n"
             << "Origin: http://inp.zoolab.org:10314\r\n"
             << "Content-Type: multipart/form-data; boundary=" << boundary << "\r\n"
             << "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/118.0.0.0 Safari/537.36\r\n"
             << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\r\n"
             << "Referer: http://inp.zoolab.org:10314/upload\r\n"
             << "Accept-Encoding: gzip, deflate\r\n"
             << "Accept-Language: zh-TW,zh;q=0.9,en-US;q=0.8,en;q=0.7\r\n\r\n"
             << "--" << boundary << "\r\n"
             << "Content-Disposition: form-data; name=\"file\"; filename=\"otp1.txt\"\r\n"
             << "Content-Type: text/plain\r\n\r\n"
             << verify << "\r\n"
             << "--" << boundary << "--\r\n";



    string request_upload = post_request.str();
    if (send(post_sock, request_upload.c_str(), request_upload.size(), 0) == -1) {
        perror("Error sending upload request");
        close(post_sock);
        return 1;
    }

    // // Send the verify string as part of the request body
    // if (send(post_sock, verify.c_str(), verify.size(), 0) == -1) {
    //     perror("Error sending verify string");
    //     close(post_sock);
    //     return 1;
    // }

    // Receive and print the response
    while ((bytes_received = recv(post_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        std::cout << buffer;
    }

    if (bytes_received == -1) {
        perror("Error receiving upload response");
    }

    close(post_sock);

    return 0;
}

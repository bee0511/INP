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
    const char* output_file = "verify.txt"; // The file to save the verify string

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
                // verify.append("%2b");
                verify.append("+");
                j++;
                continue;
            }
            
            verify.push_back(buffer[j]);
            j++;
        }
        // verify.append("%3d%3d");
        verify.append("==");
        break;
    }
    cout << endl << "Verify string: " << verify << endl;
    memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    close(sock);

ofstream verifyFile(output_file);
    if (verifyFile.is_open()) {
        verifyFile << verify;
        verifyFile.close();
    } else {
        cerr << "Error opening " << output_file << " for writing" << endl;
        return 1;
    }


    // Append the "verify" to the "path_upload"
    std::string path_upload_with_verify = "/verify?otp=" + verify;
    cout << path_upload_with_verify << endl;
    // Create a new socket for the GET request
    int otp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (otp_sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Connect to the server again
    if (connect(otp_sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(otp_sock);
        return 1;
    }

    // Construct and send the GET request to verify OTP
    std::string request_verify = "GET " + path_upload_with_verify + " HTTP/1.1\r\n" +
                              "Host: " + std::string(server) + "\r\n" +
                              "Connection: close\r\n" +
                              "\r\n";

    if (send(otp_sock, request_verify.c_str(), request_verify.size(), 0) == -1) {
        perror("Error sending verify request");
        close(otp_sock);
        return 1;
    }

    while ((bytes_received = recv(otp_sock, buffer, sizeof(buffer), 0)) > 0) {
        std::cout.write(buffer, bytes_received);
    }
    
    memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    close(otp_sock);

    return 0;
}

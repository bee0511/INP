/*UNIX Queen
This lab aims to practice implementing a UNIX domain socket client that interacts with a server. You must implement an N-Queen solver and upload the solver executable to the challenge server. To solve the puzzle, the solver must interact with the Queen server using a UNIX domain socket connection stored at /queen.sock.

The Challenge Server
The challenge server can be accessed using the nc command:

nc inp.zoolab.org 10850
You have to solve the Proof-of-Work challenge first, and then you can activate the challenge server. The challenge server has two channels to receive commands. One receives commands from the terminal, and the other receives commands from a stream-based UNIX domain socket created at the path /queen.sock (on the server). The commands and the corresponding responses are the same for both channels, but no welcome messages are sent via the UNIX domain socket. Therefore, you can play with the challenge server to get familiar with the commands and responses. With pwntools, you can play with our challenge server using the scripts play.py(view | download). You also have to download the required solpow.py(view | download). You can also use the play.py script to upload your solver (pass its path as the first argument to the script).

You are requested to solve the N-Queen challenge in this lab via the UNIX domain socket. The challenge server allows you to optionally upload a compressed N-Queen solver binary file encoded in base64. The uploaded solver can interact with the challenge server via the UNIX domain socket. The UNIX domain socket reads commands sent from a client to the server and sends the corresponding response to the client.

Command responses for cell number placement and puzzle checks requested from the UNIX domain socket are also displayed on the user terminal. Your solver program may also output messages to stdout or stderr for you to diagnose your solver implementation.

Your solver must be compiled with the -static option.
The commands used for the challenge server are listed below for your reference.

- H: Show this help.
- P: Print the current puzzle.
- M: Mark Qeeun on a cell. Usage: M [row] [col]
- C: Check the placement of the queens.
- S: Serialize the current board.
- Q: Quit.
Most commands do not have arguments except the M command. The M command places a Queen in an empty cell. Once you have filled all the N queens, you must invoke the C command to perform the check. A success message is displayed if all the queens have been filled without any conflict.*/

// The N-Queen Puzzle
// The N-Queen puzzle is a well-known puzzle in computer science. The goal of the puzzle is to place N queens on an N x N chessboard such that no two queens can attack each other. A queen can attack another queen if they are in the same row, column, or diagonal. The following figure shows a solution to the 4-Queen puzzle.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <vector>

#define MAX_N 30
#define MAX_MSG 4 * 1024
// #define DEBUG 1

int count = 0;

void sendMsg(int sock, const char* msg) {
    if (write(sock, msg, strlen(msg)) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    // print the msg to stderr
    fprintf(stderr, "send msg: %s\n", msg);
}

// receive a message from the server and print it to stderr
void recvMsg(int sock, char* buf) {
    // use loop to receive all messages from the server
    while (1) {
        int len = read(sock, buf, MAX_MSG - 1);
        // if time out, print the timeout msg
        if (len < 0) {
            // perror("read");
            // fprintf(stderr, "Timeout\n");
            return;
        }
        buf[len] = '\0';
        fprintf(stderr, "%s", buf);
    }
}

// check if the queen can be placed in the cell
bool checkqueen(int x, int y, std::vector<std::string> table, int n) {
    // fprintf(stderr, "checkqueen: %d %d\n", x, y);

    // check the diagonal
    for (int i = 0; i < n; i++) {
        if (x + i < n && y + i < n && table[x + i][y + i] == 'Q')
            return false;
        if (x - i >= 0 && y - i >= 0 && table[x - i][y - i] == 'Q')
            return false;
        if (x + i < n && y - i >= 0 && table[x + i][y - i] == 'Q')
            return false;
        if (x - i >= 0 && y + i < n && table[x - i][y + i] == 'Q')
            return false;
    }
    return true;
}

// read the string and solve the N-Queen puzzle recursively
bool solveNQueen(std::vector<std::string>& table, std::vector<bool>& row_Queen, std::vector<bool>& col_Queen, int row) {
    if (row == 30) {
        return true;
    }

    if (row_Queen[row] == true) {
        return solveNQueen(table, row_Queen, col_Queen, row + 1);
    }

    for (int col = 0; col < MAX_N; col++) {
        if (col_Queen[col] == true) {
            continue;
        }
        count++;
        if (count == 5000000) {
            fprintf(stderr, "Still solving...\n");
            count = 0;
        }
        if (checkqueen(row, col, table, MAX_N)) {
            // place the queen in the cell
            row_Queen[row] = true;
            col_Queen[col] = true;
            table[row][col] = 'Q';
            bool flg = solveNQueen(table, row_Queen, col_Queen, row + 1);
            if (flg) return true;
            row_Queen[row] = false;
            col_Queen[col] = false;
            table[row][col] = '.';
        }
    }

    return false;
}

// Set the socket and connect to the server
int init() {
    int sock;
    struct sockaddr_un server_addr;

    // Create a UNIX domain socket
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // set socket timeout to 0.5 seconds
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;  // 0.5 seconds
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, "/queen.sock");
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // output a message via stdout or stderr
    // fprintf(stderr, "Hello, world!\n");

    return sock;
}

int main() {
    int sock = init();
    char* buf;

    // Send a command to the server
    sendMsg(sock, "P\n");

    // resize buf
    buf = (char*)malloc(MAX_MSG * sizeof(char));
    // Receive the response from the server
    recvMsg(sock, buf);

    // Eliminate spaces in buf
    std::vector<std::string> str;
    std::string tmp;
    int row = 0;
    int col = 0;
    for (int i = 0; i < strlen(buf); i++) {
        if (buf[i] == '\n') {
            str.push_back(tmp);
            tmp = "";
        } else if (buf[i] == ' ') {
            continue;
        } else {
            tmp += buf[i];
        }
    }
    std::vector<bool> row_Queen(MAX_N, false);
    std::vector<bool> col_Queen(MAX_N, false);
    // check the row and column
    for (int row = 0; row < str.size(); row++) {
        for (int col = 0; col < str[row].size(); col++) {
            if (str[row][col] == 'Q') {
                row_Queen[row] = true;
                col_Queen[col] = true;
            }
        }
    }
    // print the string to stderr
    fprintf(stderr, "Before solving: \n");
    for (int i = 0; i < str.size(); i++) {
        fprintf(stderr, "%s\n", str[i].c_str());
    }

    // solve the N-Queen puzzle
    solveNQueen(str, row_Queen, col_Queen, 0);

    // print the string to stderr
    fprintf(stderr, "After solving: \n");
    for (int i = 0; i < str.size(); i++) {
        fprintf(stderr, "%s\n", str[i].c_str());
    }

    // If the table contain Q, send the Q to the server
    for (int row = 0; row < str.size(); row++) {
        for (int col = 0; col < str[row].size(); col++) {
            if (str[row][col] == 'Q') {
                // send the M command to the server
                sprintf(buf, "M %d %d\n", row, col);
                sendMsg(sock, buf);
                // receive the response from the server
                recvMsg(sock, buf);
            }
        }
    }

    // send the C command to the server
    sendMsg(sock, "C\n");
    recvMsg(sock, buf);

    return 0;
}
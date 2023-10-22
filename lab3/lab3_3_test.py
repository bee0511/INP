import socket
import time

# Constants
WALL = "#"
PATH = "."
EXIT = "E"
START = "*"
EMPTY_SPACE = " "

# Server connection setup
server_address = "inp.zoolab.org"
server_port = 10303
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((server_address, server_port))

# Initialize total_maze with the small maze in the middle
maze_size = 201
maze = [[' ' for _ in range(maze_size)] for _ in range(maze_size)]
visited = [[False for _ in range(maze_size)] for _ in range(maze_size)]
current_x = 100
current_y = 100
find_exit = False


def receiveServerReply():
    server_reply = ""
    while True:
        data = client_socket.recv(1024).decode()
        server_reply += data
        if "Enter your move(s)" in data:
            break
    print(server_reply)
    return server_reply

def mazeParser(maze_data):
    maze = [""] * 7
    lines = maze_data.split("\n")
    i = 0
    for line in lines:
        if not line.startswith("  "):
            continue
        row_data = ""
        for j in range(7, 18):
            if line[j] == 'a':
                break
            row_data += line[j]
        if row_data == "":
            continue
        maze[i] += row_data
        i = i + 1
        if i == 7:
            break
    # print(maze)
    return maze

def printMaze(maze):
    for row in maze:
        if "#" in row:
            print(''.join(row))

def getPath(maze, start_x, start_y):
    stack = [(start_x, start_y, [], False)]
    visited = [[False for _ in range(len(maze[0]))] for _ in range(len(maze))]

    while stack:
        current_x, current_y, path, find_exit = stack.pop()

        if maze[current_x][current_y] == EXIT:
            print("FIND EXIT!")
            return path, current_x, current_y, True

        if maze[current_x][current_y] == '*':
            maze[current_x][current_y] = '.'

        moves = [(-1, 0), (0, -1), (1, 0), (0, 1)]
        move_symbols = ['W', 'A', 'S', 'D']

        for move, (dx, dy) in enumerate(moves):
            new_x, new_y = current_x + dx, current_y + dy
            if maze[new_x][new_y] == " ":
                return path, current_x, current_y, False
            if maze[new_x][new_y] == "E":
                print("FIND EXIT!!!")
                return path + [move_symbols[move]], new_x, new_y, True
            if (
                0 <= new_x < len(maze)
                and 0 <= new_y < len(maze[0])
                and maze[new_x][new_y] != WALL
                and not visited[new_x][new_y]
            ):
                visited[new_x][new_y] = True
                stack.append((new_x, new_y, path + [move_symbols[move]], find_exit))
    
    return None, current_x, current_y, find_exit

def updateMaze(path, current_x, current_y, find_exit):
    # print(current_x, current_y)
    path = "".join(path)
    path += '\n'
    # print(path)
    client_socket.sendall(path.encode())
    time.sleep(0.1)
    
    if find_exit:
        print("Found EXIT! RECEIVING")
        for _ in range(10):
            data = client_socket.recv(1024).decode()
            print(data)
        return
    # Receive the updated maze from the server
    server_reply = receiveServerReply()
    small_maze = mazeParser(server_reply)

    # Update the maze based on the path and new small maze
    printMaze(small_maze)
    for i in range(len(small_maze)):
        for j in range(len(small_maze[0])):
            # print(current_x - len(small_maze) // 2 + i, current_y - len(small_maze[0]) // 2 + j)
            maze[current_x - len(small_maze) // 2 + i][current_y - len(small_maze[0]) // 2 + j] = small_maze[i][j]


server_reply = receiveServerReply()
small_maze = mazeParser(server_reply)
for i in range(len(small_maze)):
        for j in range(len(small_maze[0])):
            maze[current_x - len(small_maze) // 2 + i][current_y - len(small_maze[0]) // 2 + j] = small_maze[i][j]
# Main loop
while True:
    visited = [[False for _ in range(maze_size)] for _ in range(maze_size)]
    if find_exit: break
    if START in [cell for row in maze for cell in row]:
        path, current_x, current_y, find_exit = getPath(maze, current_x, current_y)
        # print(path)
        updateMaze(path, current_x, current_y, find_exit)
        printMaze(maze)
        
client_socket.close()

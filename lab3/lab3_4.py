import socket
import time

# Constants
WALL = "#"
PATH = "."
EXIT = "E"
START = "*"

# Server connection setup
server_address = "inp.zoolab.org"
server_port = 10304
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((server_address, server_port))

# Initialize total_maze with the small maze in the middle
total_maze_size = 201
total_maze = [[' ' for _ in range(total_maze_size)] for _ in range(total_maze_size)]

# Initial position for the small maze
small_maze_x = total_maze_size // 2 - 3
small_maze_y = total_maze_size // 2 - 5

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
        if not line.startswith("   "):
            continue
        row_parts = line.split(": ")
        row_data = row_parts[1].strip()
        if row_data == "":
            break
        maze[i] += row_data
        i = i + 1
        if i == 7:
            break
    return maze

def printMaze(maze):
    for row in maze:
        print(''.join(row))

def dfs_explore(maze, x, y, path, visited):
    if x == 0 or y == 0 or x == len(maze) - 1 or y == len(maze[0]) - 1:
        return path

    moves = [(-1, 0), (0, -1), (1, 0), (0, 1)]
    move_symbols = ['W', 'A', 'S', 'D']
    
    for move, (dx, dy) in enumerate(moves):
        new_x, new_y = x + dx, y + dy
        if 0 <= new_x < len(maze) and 0 <= new_y < len(maze[0]) and maze[new_x][new_y] != WALL and not visited[new_x][new_y]:
            visited[new_x][new_y] = True
            path.append(move_symbols[move])
            result = dfs_explore(maze, new_x, new_y, path, visited)
            if result is not None:
                return result
            path.pop()

    return None

def solveMazeWithDFS(maze):
    # Set the initial position to the middle of the small maze
    start_x, start_y = len(maze) // 2, len(maze[0]) // 2

    # Explore the small maze starting from the initial position
    visited = [[False for _ in range(len(maze[0]))] for _ in range(len(maze))]
    visited[start_x][start_y] = True
    path = dfs_explore(maze, start_x, start_y, [], visited)

    if path is None:
        print("No valid path found to the border of the small maze.")
        return []

    # Send the moves to the server
    path = ''.join(path)
    path += "\n"
    client_socket.sendall(path.encode())
    print("Sent moves to the server:", path)
    time.sleep(0.3)

    # Receive the updated maze from the server
    server_reply = receiveServerReply()
    maze_data = mazeParser(server_reply)
    print("Received updated maze:")
    printMaze(maze_data)

    # Update the total maze with the new information based on the path
    current_x, current_y = small_maze_x, small_maze_y
    for move in path:
        if move == 'W':
            current_x -= 1
        elif move == 'A':
            current_y -= 1
        elif move == 'S':
            current_x += 1
        elif move == 'D':
            current_y += 1

        for i in range(len(maze_data)):
            for j in range(len(maze_data[0])):
                total_maze[current_x - len(maze_data) // 2 + i][current_y - len(maze_data[0]) // 2 + j] = maze_data[i][j]

    return total_maze


# Main loop
while True:
    server_reply = receiveServerReply()
    maze_data = mazeParser(server_reply)
    if START in [cell for row in maze_data for cell in row]:
        total_maze = solveMazeWithDFS(maze_data)
        print("Current state of the total maze:")
        printMaze(total_maze)
    else:
        print("No start found in the maze.")
        break

client_socket.close()

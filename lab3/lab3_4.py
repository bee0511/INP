import socket
import time

# Constants
WALL = "#"
PATH = "."
EXIT = "E"
START = "*"

# Server connection setup
server_address = "your_server_address"
server_port = 12345
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((server_address, server_port))

def receive_server_reply():
    server_reply = ""
    while True:
        data = client_socket.recv(1024).decode()
        server_reply += data
        if "Enter your move(s)" in data:
            break
    return server_reply

def maze_parser_2(maze_data):
    lines = maze_data.split('\n')
    maze = [list(line) for line in lines]
    width = len(maze[0])
    height = len(maze)
    return maze, width, height

def dfs_explore(maze, x, y, path, visited):
    moves = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    move_symbols = ['W', 'A', 'S', 'D']

    for move, (dx, dy) in enumerate(moves):
        new_x, new_y = x + dx, y + dy
        if 0 <= new_x < len(maze) and 0 <= new_y < len(maze[0]) and maze[new_x][new_y] != WALL and not visited[new_x][new_y]:
            visited[new_x][new_y] = True
            path.append(move_symbols[move])
            dfs_explore(maze, new_x, new_y, path, visited)
            path.pop()

def solveMazeWithDFS(maze):
    start_x, start_y = None, None
    for i in range(len(maze)):
        for j in range(len(maze[0])):
            if maze[i][j] == START:
                start_x, start_y = i, j

    # Create a larger maze to store the total exploration
    total_maze_size = 201
    total_maze = [['#' for _ in range(total_maze_size)] for _ in range(total_maze_size)]

    # Start in the middle of the total maze
    current_x, current_y = total_maze_size // 2, total_maze_size // 2

    # Initialize visited and path lists
    visited = [[False for _ in range(len(maze[0]))] for _ in range(len(maze))]
    path = []

    # Explore the 11x7 maze and update the total maze
    dfs_explore(maze, start_x, start_y, path, visited)

    for move in path:
        if move == 'W':
            current_x -= 1
        elif move == 'A':
            current_y -= 1
        elif move == 'S':
            current_x += 1
        elif move == 'D':
            current_y += 1

    # Send the moves to the server
    client_socket.sendall(''.join(path).encode())
    print("Sent moves to server:", ''.join(path))
    
    # Receive the updated maze from the server
    server_reply = receive_server_reply()
    server_maze, _, _ = maze_parser_2(server_reply)

    # Update the total maze with the new information
    for i in range(len(server_maze)):
        for j in range(len(server_maze[0])):
            total_maze[current_x - len(server_maze) // 2 + i][current_y - len(server_maze[0]) // 2 + j] = server_maze[i][j]

    return total_maze

# Main loop
while True:
    server_reply = receive_server_reply()
    maze, width, height = maze_parser_2(server_reply)
    
    if START in [cell for row in maze for cell in row]:
        total_maze = solveMazeWithDFS(maze)
        # Print the current state of the total maze (for debugging)
        for row in total_maze:
            print(''.join(row))
    else:
        print("No start found in the maze.")
        break

client_socket.close()

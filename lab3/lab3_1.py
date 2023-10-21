import socket
from MazeSolver import solveMazeWithAStar
import time

def print_maze(data):
    # Split the data by newlines
    lines = data.split("\n")

    # Extract the maze size from Note1
    size_line = [line for line in lines if "Size of the maze" in line][0]
    size_text = size_line.split("= ")[-1]
    width, height = map(int, size_text.split(" x "))

    # Extract the maze layout (including borders and excluding notes)
    maze_lines = [line for line in lines if line.startswith("#")]

    # Print the extracted maze size and layout
    print("Maze Size (width x height):", width, "x", height)
    print("Maze Layout:")
    for line in maze_lines:
        print(line)

def maze_parser_1(data):
    # Split the data by newlines
    lines = data.split("\n")

    # Extract the maze size from Note1
    size_line = [line for line in lines if "Size of the maze" in line][0]
    size_text = size_line.split("= ")[-1]
    width, height = map(int, size_text.split(" x "))

    # Extract the maze layout (including borders and excluding notes)
    maze_lines = [line for line in lines if line.startswith("#")]
    return maze_lines, width, height

def solve_maze_1():
    server_address = "inp.zoolab.org"
    server_port = 10301

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((server_address, server_port))

    # Wait for the server to send maze information
    server_reply = ""
    while True:
        data = client_socket.recv(1024).decode()
        server_reply += data
        if "Enter your move(s)" in data:
            break
    print(server_reply)
    
    print_maze(server_reply)
    maze, width, height = maze_parser_1(server_reply)
    moves = solveMazeWithAStar(maze)
    print("the move is:", moves)
    client_socket.sendall(moves.encode())
    for _ in range(3):
        data = client_socket.recv(1024).decode()
        print(data)
    
    # Close the connection
    client_socket.close()

def main():
    print("Solving Maze #1")
    solve_maze_1()

if __name__ == "__main__":
    main()

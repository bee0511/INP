import socket
import heapq
import time

def recvline(s):
    data = b''
    while not data.endswith(b'\n'):
        data += s.recv(1)
    return data

def recvall(s, delim):
    data = b''
    while not data.endswith(delim):
        data += s.recv(1024)
    return data

def solve_maze_with_a_star(maze):
    def heuristic(position, exit):
        # Calculate the Manhattan distance as a heuristic
        return abs(position[0] - exit[0]) + abs(position[1] - exit[1])

    def get_neighbors(position):
        x, y = position
        neighbors = []
        for dx, dy in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
            new_x, new_y = x + dx, y + dy
            if 0 <= new_x < width and 0 <= new_y < height and maze[new_y][new_x] != '#':
                neighbors.append((new_x, new_y))
        return neighbors

    start, exit = None, None
    width, height = len(maze[0]), len(maze)
    
    for y in range(height):
        for x in range(width):
            if maze[y][x] == '*':
                start = (x, y)
            elif maze[y][x] == 'E':
                exit = (x, y)

    if start is None or exit is None:
        return []

    open_list = [(0, start)]
    closed_list = set()
    parent = {}
    cost = {start: 0}

    while open_list:
        current_cost, current_position = heapq.heappop(open_list)

        if current_position == exit:
            path = [current_position]
            while path[-1] != start:
                path.append(parent[path[-1]])
            path.reverse()
            return path_to_moves(path)

        closed_list.add(current_position)

        for neighbor in get_neighbors(current_position):
            if neighbor in closed_list:
                continue

            new_cost = cost[current_position] + 1
            if neighbor not in [pos for _, pos in open_list] or new_cost < cost[neighbor]:
                cost[neighbor] = new_cost
                priority = new_cost + heuristic(neighbor, exit)
                heapq.heappush(open_list, (priority, neighbor))
                parent[neighbor] = current_position

    return []

def path_to_moves(path):
    moves = []
    for i in range(1, len(path)):
        x1, y1 = path[i-1]
        x2, y2 = path[i]
        if x2 > x1:
            moves.append('D')
        elif x2 < x1:
            moves.append('A')
        elif y2 > y1:
            moves.append('S')
        elif y2 < y1:
            moves.append('W')
    return moves

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

def solve_maze_2():
    server_address = "inp.zoolab.org"
    server_port = 10302

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
    

    maze, width, height = maze_parser_1(server_reply)
    moves = solve_maze_with_a_star(maze)
    move_str = ""
    for move in moves:
        move_str += move
    move_str += "\n"
    print("the move is:", move_str)
    client_socket.sendall(move_str.encode())
    for _ in range(3):
        data = client_socket.recv(1024).decode()
        print(data)
    
    # Close the connection
    client_socket.close()

def main():
    print("Solving Maze #2")
    solve_maze_2()

if __name__ == "__main__":
    main()

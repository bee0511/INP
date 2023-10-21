import socket
from MazeSolver import solveMazeWithAStar
import time

def moveToUpleftCorner(lines, v_width, v_height, client_socket):
    top_row_num = 0
    bottom_row_num = 0
    for line in lines:
        if line.startswith("   "):
            row_parts = line.split(": ")
            row_number = int(row_parts[0].strip())
            row_data = row_parts[1].strip()
            # Check if the row_data has extra spaces
            if top_row_num == 0 and row_number >= 0:
                top_row_num = row_number
                bottom_row_num = row_number
            if row_number > bottom_row_num:
                bottom_row_num = row_number
            # print (row_number, row_data)
            if v_width > len(row_data) :
                # Reached the border of the maze
                move_back = v_width - len(row_data)
                move_back_str = ""
                for _ in range(move_back):
                    move_back_str += "l"
                move_back_str += "\n"
                client_socket.sendall(move_back_str.encode())
                data = client_socket.recv(1024).decode()
                break
    move_up_down_str = ""
    if top_row_num == 0 and bottom_row_num - top_row_num < v_height:
        move_down = v_height - (bottom_row_num - top_row_num)
        for _ in range(move_down):
            move_up_down_str += "k"
    elif top_row_num != 0:
        move_up = top_row_num
        for _ in range(move_up):
            move_up_down_str += "i"
    move_up_down_str += "\n"
    client_socket.sendall(move_up_down_str.encode())
    data = client_socket.recv(1024).decode()
    previous_length = -1
    lines = data.split("\n")
    for line in lines:
        if line.startswith("   "):
            row_parts = line.split(": ")
            row_number = int(row_parts[0].strip())
            row_data = row_parts[1].strip()
            previous_length = len(row_data)
    current_length = -1
    client_socket.sendall("j\n".encode())
    data = client_socket.recv(1024).decode()
    lines = data.split("\n")
    for line in lines:
        if line.startswith("   "):
            row_parts = line.split(": ")
            row_number = int(row_parts[0].strip())
            row_data = row_parts[1].strip()
            current_length = len(row_data)
    if current_length < previous_length:
        # in the left most maze
        client_socket.sendall("lllllllllll\n".encode())
        data = client_socket.recv(1024).decode()
    elif current_length > previous_length:
        # in the right most maze
        client_socket.sendall("jjjjjjjjjjj\n".encode())
        data = client_socket.recv(1024).decode()
    client_socket.sendall("l\n".encode())
    data = client_socket.recv(1024).decode()
    time.sleep(0.1)
    while(True):
        client_socket.sendall("jjjjjjjjjjj\n".encode())
        time.sleep(0.1)
        tmp_data = client_socket.recv(1024).decode()
        lines = tmp_data.split("\n")
        row_data = []
        for line in lines:
            if line.startswith("   "):
                row_parts = line.split(": ")
                row_number = int(row_parts[0].strip())
                row_data = row_parts[1].strip()
                
                # print (row_number, row_data)
        if v_width > len(row_data) :
            # Reached the border of the maze, move back
            move_back = v_width - len(row_data)
            # print("move back:",move_back)
            move_back_str = ""
            for _ in range(move_back):
                move_back_str += "l"
            move_back_str += "\n"
            client_socket.sendall(move_back_str.encode())
            time.sleep(0.1)
            data = client_socket.recv(1024).decode()
            print("After: ")
            print(data)
            break
    return data

def getMazeRow(client_socket, initial_data):
    maze = [""] * 7  # Initialize a list with 7 empty strings

    lines = initial_data.split("\n")
    i = 0
    for line in lines:
        if not line.startswith("   "):
            continue
        row_parts = line.split(": ")
        row_data = row_parts[1].strip()
        # print(row_data)
        if row_data == "": break
        maze[i] += row_data
        i = i + 1
        if i == 7: break
    
    for _ in range(9):
        client_socket.sendall("lllllllllll\n".encode())
        time.sleep(0.1)
        tmp_data = client_socket.recv(1024).decode()
        lines = tmp_data.split("\n")
        i = 0
        for line in lines:
            if not line.startswith("   "):
                continue
            row_parts = line.split(": ")
            row_data = row_parts[1].strip()
            # print(row_data)
            if row_data == "": break
            maze[i] += row_data
            i = i + 1
            if i == 7: break

    for line in maze:
        print(line)
    
    return maze




def maze_parser_3(data, client_socket):
    maze = [""] * 101
    maze[100] = "#####################################################################################################"
    # Split the data by newlines
    lines = data.split("\n")
    # Extract the maze size from Note1
    size_line = [line for line in lines if "Size of the maze" in line][0]
    size_text = size_line.split("= ")[-1]
    m_width, m_height = map(int, size_text.split(" x "))

    # Extract the maze size from Note1
    View_line = [line for line in lines if "View port area" in line][0]
    View_size = View_line.split("= ")[-1]
    v_width, v_height = map(int, View_size.split(" x "))

    for i in range(15):
        client_socket.sendall("r\n".encode())
        time.sleep(0.1)
        client_socket.recv(1024).decode()
        moveToUpleftCorner(lines, v_width, v_height, client_socket)
        move_down_str = ""
        for _ in range(7 * i):
            move_down_str += "k"
        move_down_str += "\n"
        client_socket.sendall(move_down_str.encode())
        time.sleep(0.1)
        initial_data = client_socket.recv(1024).decode()

        tmp_row = getMazeRow(client_socket, initial_data)
        for j in range(7):
            if (i * 7) + j > 100: break
            maze[i*7 + j] += tmp_row[j]
            
        # print(maze)
    for line in maze:
        print(line)
    return maze

def solve_maze_3():
    server_address = "inp.zoolab.org"
    server_port = 10303

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
    

    maze = maze_parser_3(server_reply, client_socket)
    moves = solveMazeWithAStar(maze)
    print("the move is:", moves)
    client_socket.sendall(moves.encode())
    for _ in range(4):
        data = client_socket.recv(1024).decode()
        print(data)
    
    # Close the connection
    client_socket.close()

def main():
    print("Solving Maze #3")
    solve_maze_3()

if __name__ == "__main__":
    main()

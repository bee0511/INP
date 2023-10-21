import heapq

def path_to_moves(path):
    moves = ""
    for i in range(1, len(path)):
        x1, y1 = path[i-1]
        x2, y2 = path[i]
        if x2 > x1:
            moves += 'D'
        elif x2 < x1:
            moves += 'A'
        elif y2 > y1:
            moves += 'S'
        elif y2 < y1:
            moves += 'W'
    moves += '\n'
    return moves


def solveMazeWithAStar(maze):
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
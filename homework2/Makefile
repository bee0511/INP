# Compile and link the program hw2_chat_server.cpp

# Compiler
CC = g++

# Compiler flags
CFLAGS = -Wall -Wextra -Wpedantic -std=c++17

# Linker flags
LFLAGS = 

# Target
TARGET = hw2_chat_server server chatroom

# Source files
SRC = hw2_chat_server.cpp server.cpp chatroom.cpp

# Object files
OBJ = $(SRC:.cpp=.o)

# Compile and link
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

# Clean
clean:
	rm -f $(OBJ) $(TARGET)

# Run
run:	hw2_chat_server
	./hw2_chat_server 8080
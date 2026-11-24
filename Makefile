CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LDFLAGS =

OBJ = obj/main.o obj/shell.o obj/execute.o
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

obj/%.o: src/%.c include/shell.h
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin


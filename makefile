SRC=./src
OBJ=./obj
CC = gcc
DEBUG = -g
NOLINK = -c
CCFLAGS = -Wall -pedantic -Wextra -Wfloat-equal -Wundef \
		  -Wshadow -Wpointer-arith -Wcast-align -Wconversion \
		  -Wswitch-default -Wcast-qual -Wwrite-strings -Waggregate-return -std=c11

.PHONY: all

all: $(OBJ) server $(OBJ) player

# Creating folder
$(OBJ):
	mkdir -p $(OBJ)

$(SRC):
	mkdir -p $(SRC)

# Dynamic linking with ncurses
server: $(OBJ)/main.o $(OBJ)/map.o
	$(CC) $(DEBUG) $(CCFLAGS) $^ -o $@ -lncurses -lpthread

player: $(OBJ)/player.o $(OBJ)/map.o
	$(CC) $(DEBUG) $(CCFLAGS) $^ -o $@ -lncurses -lpthread

# .O files with debugging symbols
$(OBJ)/main.o: $(SRC)/main.c
	$(CC) $(DEBUG) $(NOLINK) $(CCFLAGS) $< -o $@

$(OBJ)/map.o: $(SRC)/map.c $(SRC)/map.h
	$(CC) $(DEBUG) $(NOLINK) $(CCFLAGS) $< -o $@

$(OBJ)/player.o: $(SRC)/player.c $(SRC)/player.h
	$(CC) $(DEBUG) $(NOLINK) $(CCFLAGS) $< -o $@

.PHONY: clean

clean:
	-rm server $(OBJ)/*.o

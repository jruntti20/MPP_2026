CC=g++
CFLAGS=-g -Wall
LIB = -L /usr/local/cuda-12.1/lib64 -lOpenCL
INC = -I /usr/local/include

SRC=src
OBJ=obj
SRCS=$(wildcard $(SRC)/*.cpp $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/*.cpp, $(OBJ)/%.o, $(SRCS))
BIN=bin/main

main: $(BIN)

$(BIN): $(OBJS) $(OBJ)
	$(CC) $(LIB) $(INC) $(CFLAGS) $(OBJS) -o $@

$(OBJ)/%.o: $(SRC)/%.cpp  $(SRC)/%.c $(OBJ)
	$(CC) $(LIB) $(INC) $(CFLAGS) -c $^ -o $@

$(OBJ):
	mkdir -p $@

clean:
	$(RM) -r $(OBJ)
	$(RM) $(BIN)


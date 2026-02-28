CC=g++
CFLAGS=-g3 -Wall -DCL_TARGET_OPENCL_VERSION=300
#LIB = -L /usr/local/cuda-12.1/lib64 -lOpenCL
#INC = -I /usr/local/cuda-12.1/include
LIB = -L /opt/cuda/lib64 -lOpenCL
INC = -I /opt/cuda/include

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


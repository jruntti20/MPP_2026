CC=g++ -fopenmp
CFLAGS=-g -Wall -O2
#CFLAGS=-g -Wall -O0
#LIB = -L /opt/cuda/lib64 -lOpenCL
#INC = -I /opt/cuda/include

SRC=src
OBJ=obj
SRCS=$(wildcard $(SRC)/*.cpp $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/*.cpp, $(OBJ)/%.o, $(SRCS))
BIN=bin/main

main: $(BIN)

test: $(SRC)/test.c 
	gcc $(LIB) $(INC) $(CFLAGS) $(SRC)/test.c -o bin/test 


$(BIN): $(OBJS) $(OBJ)
	$(CC) $(LIB) $(INC) $(CFLAGS) $(OBJS) -o $@

$(OBJ)/%.o: $(SRC)/%.cpp $(SRC)/%.c $(OBJ)
	$(CC) $(LIB) $(INC) $(CFLAGS) -c $< -o $@

$(OBJ):
	mkdir -p $@

clean:
	$(RM) -r $(OBJ)
	$(RM) $(BIN)


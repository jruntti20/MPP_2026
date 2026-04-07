CXX = g++
CXXFLAGS = -g -std=c++17 -Wall -O2
#LIBS = -lOpenCL
LIBS = -L /opt/cuda/lib64 -lOpenCL
INC = -I /opt/cuda/include


#SRC = main.cpp \
      imageManipulation.c \
      cross_checking.cpp \
      occlusion_filling.cpp \
      lodepng.cpp

SRC=src
      
#OBJ = $(SRC:.cpp=.o)
#OBJ := $(OBJ:.c=.o)
OBJ=obj
SRCS=$(wildcard $(SRC)/*.cpp $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/*.cpp, $(OBJ)/%.o, $(SRCS))
BIN=bin

TARGET = $(BIN)/stereo_opencl

all: $(TARGET)

$(TARGET): $(OBJS) $(OBJ)
	$(CXX) $(LIBS) $(INC) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ)/%.o: $(SRC)/%.cpp $(SRC)/%.c $(OBJ)
	$(CXX) $(LIBS) $(INC) $(CXXFLAGS) -c $< -o $@

$(OBJ):
	mkdir -p $@

clean:
	$(RM) -r $(OBJ)
	$(RM) $(TARGET)


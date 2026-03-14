CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
LIBS = -lOpenCL

TARGET = stereo_opencl

SRC = main.cpp \
      imageManipulation.c \
      cross_checking.cpp \
      occlusion_filling.cpp \
      lodepng.cpp

OBJ = $(SRC:.cpp=.o)
OBJ := $(OBJ:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o $(TARGET)
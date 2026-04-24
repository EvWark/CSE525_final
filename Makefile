CXX = g++
CXXFLAGS = -Wall -std=c++17 -O2

SRC = main.cpp brain.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = game

INCLUDES = -I./include

LIBS = -lwiringPi -lpthread

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)
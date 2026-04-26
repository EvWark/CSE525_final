CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2

SRC      = braintest.cpp brain.cpp
OBJ      = $(SRC:.cpp=.o)
TARGET   = brain_serial_test

INCLUDES = -I./include
LIBS     = -lpthread

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

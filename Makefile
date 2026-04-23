# ===== Compiler =====
CXX = g++
CXXFLAGS = -Wall -std=c++17 -O2

# ===== Project structure =====
SRC = main.cpp
OBJ = main.o
TARGET = game

# ===== External includes =====
INCLUDES = -I./include

# ===== Libraries =====
LIBS = -lwiringPi

# ===== Build rules =====

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

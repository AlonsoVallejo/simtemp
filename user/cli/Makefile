CXX = g++
CXXFLAGS = -Wall -Wextra -O2
TARGET = simtemp_cli
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

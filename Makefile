CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I./include

TARGET = tun
SRCS   = $(wildcard src/*.cpp)
OBJS   = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	
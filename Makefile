CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -g

LDFLAGS =

TARGET = peripheral_slow

SRCS = main.cpp peripheral.cpp slow.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: main.cpp peripheral.h slow.h
peripheral.o: peripheral.cpp peripheral.h slow.h
slow.o: slow.cpp slow.h

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
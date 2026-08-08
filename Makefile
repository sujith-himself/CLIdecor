CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra

TARGET = clidecor

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    TARGET := clidecor.exe
endif

all: $(TARGET)

$(TARGET): src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) clidecor clidecor.exe

.PHONY: all clean

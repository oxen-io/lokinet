
SRC = $(sort $(wildcard src/*.cpp))
OBJS = $(SRC:.cpp=.cpp.o)

CGI_EXE = lokinet-bootserv

CXX = clang++

all: build

build: $(CGI_EXE)

%.cpp.o: %.cpp
	$(CXX) -g -std=c++17 -c -Wall -Werror -Wpedantic $^ -o $^.o

$(CGI_EXE): $(OBJS)
	$(CXX) -o $(CGI_EXE) $^

clean:
	rm -f $(CGI_EXE) $(OBJS)

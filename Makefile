CC=gcc
CCOPTS=-g -Wall -D_GNU_SOURCE -D_XOPEN_SOURCE -Iinclude
CXX=g++
CXXOPTS=-g -Wall -D_GNU_SOURCE -D_XOPEN_SOURCE -Iinclude -std=gnu++0x
LD=ld
LDLIBS=-lm
LDFLAGS=

all: server client

clean:
	rm -f *.o server client

server: server.o
	$(CXX) $(CXXOPTS) -o server server.o

server.o: server.cpp util.hpp
	$(CXX) $(CXXOPTS) -c server.cpp

client: client.o
	$(CXX) $(CXXOPTS) client.o -o client

client.o: client.cpp
	$(CXX) $(CXXOPTS) -c client.cpp

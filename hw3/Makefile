CC = gcc
FLAGS =  -Wall -Werror -std=c89
LIBS =

# server client
SERVER = server
CLIENT = client
LIB = libapi.so

TARGETS = $(LIB)

all: $(TARGETS)

libapi.so: api.o api.h
	$(CC) $(FLAGS) -shared -o $(LIB) api.o

api.o: api.c api.h
	$(CC) $(FLAGS) -fPIC -c api.c

clean:
	rm -f *.o $(TARGETS)

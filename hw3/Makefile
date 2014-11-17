CC = gcc
FLAGS =  -Wall -Werror -std=c89 -DCOLOR
LIBS =

SERVER = server
CLIENT = client
LIB = libapi.a

TARGETS = $(LIB)

all: $(TARGETS)

$(LIB): api.o api.h
	ar -cvq $(LIB) api.o

api.o: api.c api.h
	$(CC) $(FLAGS) -c api.c

clean:
	rm -f *.o $(TARGETS)

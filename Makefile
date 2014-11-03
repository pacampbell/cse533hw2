include Makefile.common

all: src
all: $(PROJECTS)

debug: CFLAGS += -DDEBUG
debug: src
debug: $(PROJECTS)

src: $(SRC)
	$(CC) $(IINC) $(CFLAGS) -c $^

$(SERVER): $(SERVER_DEPS)
	$(CC) $(IINC) $(CFLAGS) -o $(SERVER) $(SERVER_DEPS) $(LIBS)

$(CLIENT): $(CLIENT_DEPS)
	$(CC) $(IINC) $(CFLAGS) -o $(CLIENT) $(CLIENT_DEPS) $(LIBS)

run-$(SERVER):
	./$(SERVER)

run-$(CLIENT):
	./$(CLIENT)

clean:
	rm -f *.o $(SERVER) $(CLIENT)

handin:
	/home/courses/cse533/handin 2 Makefile Makefile.common README.md $(SRC) $(HDR)

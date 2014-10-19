include Makefile.common

all: $(shell mkdir -p $(BINDIR))
all: $(PROJECTS)

debug: CFLAGS := -DDEBUG
debug: $(shell mkdir -p $(BINDIR))
debug: $(PROJECTS)

server-debug: CFLAGS := -DDEBUG
server-debug: $(shell mkdir -p $(BINDIR))
server-debug: $(SERVER)

client-debug: CFLAGS := -DDEBUG
client-debug: $(shell mkdir -p $(BINDIR))
client-debug: $(CLIENT)

$(SERVER): $(SRC)/server/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(SERVER) $(SRC)/server/*.c

$(CLIENT): $(SRC)/client/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(CLIENT) $(SRC)/client/*.c

clean:
	rm -rf $(BINDIR)/* *.o

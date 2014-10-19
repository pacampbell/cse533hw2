include Makefile.common

all: $(shell mkdir -p $(BINDIR))
all: $(PROJECTS)

debug: CFLAGS := -DDEBUG
debug: $(shell mkdir -p $(BINDIR))
debug: $(PROJECTS)

$(SERVER)-debug: CFLAGS := -DDEBUG
$(SERVER)-debug: $(shell mkdir -p $(BINDIR))
$(SERVER)-debug: $(SERVER)

$(CLIENT)-debug: CFLAGS := -DDEBUG
$(CLIENT)-debug: $(shell mkdir -p $(BINDIR))
$(CLIENT)-debug: $(CLIENT)

$(SERVER): $(SRC)/server/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(SERVER) $(SRC)/server/*.c

$(CLIENT): $(SRC)/client/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(CLIENT) $(SRC)/client/*.c

clean:
	rm -rf $(BINDIR)/* *.o

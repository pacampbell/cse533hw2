include Makefile.common

all: $(shell mkdir -p $(BINDIR))
all: $(PROJECTS)

debug: CFLAGS := -DDEBUG
debug: $(MAKE_BIN)
debug: $(PROJECTS)

$(SERVER)-debug: CFLAGS := -DDEBUG
$(SERVER)-debug: $(MAKE_BIN)
$(SERVER)-debug: $(SERVER)

$(CLIENT)-debug: CFLAGS := -DDEBUG
$(CLIENT)-debug: $(MAKE_BIN)
$(CLIENT)-debug: $(CLIENT)

$(SERVER): $(SRC)/server/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(SERVER) $(SRC)/server/*.c

$(CLIENT): $(SRC)/client/*.c
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(CLIENT) $(SRC)/client/*.c

run-$(SERVER):
	@./$(BINDIR)/$(SERVER)

run-$(CLIENT):
	@./$(BINDIR)/$(CLIENT)

clean:
	rm -rf $(BINDIR)/*

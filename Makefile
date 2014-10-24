include Makefile.common

all: $(MAKE_BIN)
all: $(PROJECTS)

debug: CFLAGS += -DDEBUG
debug: $(MAKE_BIN)
debug: $(PROJECTS)

$(SERVER)-debug: CFLAGS += -DDEBUG
$(SERVER)-debug: $(MAKE_BIN)
$(SERVER)-debug: $(SERVER)

$(CLIENT)-debug: CFLAGS += -DDEBUG
$(CLIENT)-debug: $(MAKE_BIN)
$(CLIENT)-debug: $(CLIENT)

$(SERVER): $(SRC)/server/*.c $(SRC)/common/*.c $(IFI)
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(SERVER) $(SRC)/server/*.c $(SRC)/common/*.c $(IFI)/$(IFI_FILE).o $(LIBS)

$(CLIENT): $(SRC)/client/*.c $(SRC)/common/*.c $(IFI)
	$(CC) $(IINC) $(CFLAGS) -o $(BINDIR)/$(CLIENT) $(SRC)/client/*.c $(SRC)/common/*.c $(IFI)/$(IFI_FILE).o $(LIBS)

$(IFI): $(IFI)/$(IFI_FILE).c
	$(CC) $(IINC) $(CFLAGS) -c $(IFI)/$(IFI_FILE).c $(LIBS)

run-$(SERVER):
	@cd $(BINDIR) && ./$(SERVER)

run-$(CLIENT):
	@cd $(BINDIR) && ./$(CLIENT)

clean:
	rm -rf $(BINDIR)/*

CC      = cc
BIN     = sheet
SRC     = sheet.c
CFLAGS  = -O2 -march=native
LDFLAGS = -lncurses -ltinfo
PREFIX  = /usr

.PHONY: all install uninstall clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN)

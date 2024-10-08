CFLAGS?=-W -Wall -Wextra -O2
CFLAGS+=$(shell gfxprim-config --cflags)
BIN=gpcoderead
SOURCES=$(wildcard *.c)
$(BIN): LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets --libs-grabbers) -lzbar
DEP=$(SOURCES:.c=.dep)

all: $(BIN) $(DEP)

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

-include: $(DEP)

install:
	install -m 644 -D layout.json $(DESTDIR)/etc/gp_apps/$(BIN)/layout.json
	install -D $(BIN) -t $(DESTDIR)/usr/bin/

clean:
	rm -f $(BIN) *.dep *.o


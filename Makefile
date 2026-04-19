# muhhmenu - Makefile

include config.mk

SRC = src/muhhmenu.c \
      src/state.c \
      src/x11.c \
      src/draw.c \
      src/input.c \
      src/frecency.c \
      src/items.c \
      drw.c \
      util.c

OBJ = $(SRC:.c=.o)

all: muhhmenu

muhhmenu: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c muhhmenu.h config.h
	$(CC) -c $(CFLAGS) -o $@ $<

config.h:
	cp config.def.h config.h

clean:
	rm -f muhhmenu $(OBJ)

install: muhhmenu
	mkdir -p $(BINDIR)
	cp -f muhhmenu $(BINDIR)
	chmod 755 $(BINDIR)/muhhmenu

uninstall:
	rm -f $(BINDIR)/muhhmenu

.PHONY: all clean install uninstall

# config.mk - build configuration for muhhmenu

CC      = cc
VERSION = 0.1

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1

INCS = -I. -Isrc \
       $(shell pkg-config --cflags x11 xft fontconfig)

LIBS = $(shell pkg-config --libs x11 xft fontconfig) \
       -lsqlite3

CFLAGS   = -std=c99 -pedantic -Wall -Wextra -g -O0 $(INCS) \
           -DVERSION=\"$(VERSION)\"
LDFLAGS  = $(LIBS)

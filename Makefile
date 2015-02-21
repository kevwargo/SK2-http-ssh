INCDIR=include
OBJDIR=obj
DEBUGDIR=debug
SRCDIR=src

CC=gcc
CFLAGS=-c -std=c99 -march=native -O2 -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE -I$(INCDIR)
DEBUG_CFLAGS=-c -g -std=c99 -march=native -O0 -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE -I$(INCDIR)
LD=gcc
LDFLAGS=-s
DEBUG_LDFLAGS=-g


LIBS=-pthread

_DEPS = clientlist.h httprequest.h httpresponse.h httpserver.h misc.h socket-helpers.h httpmessage.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = clientlist.o httpmessage.o httprequest.o httpresponse.o httpserver.o main.o misc.o socket-helpers.o
OBJ = $(patsubst %,$(OBJDIR)/%,$(_OBJ))
DEBUGOBJ = $(patsubst %,$(DEBUGDIR)/%,$(_OBJ))
# OBJ=$(OBJDIR)/server.o $(OBJDIR)/address.o $(OBJDIR)/clientlist.o

server: $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) $< -o $@

$(DEBUGDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) $(DEBUG_CFLAGS) $< -o $@

debug: $(DEBUGOBJ)
	$(LD) $(DEBUG_LDFLAGS) $(LIBS) $^ -o server-debug


.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o *~ $(INCDIR)/*~

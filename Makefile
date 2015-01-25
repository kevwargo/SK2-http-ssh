INCDIR=include
OBJDIR=obj
SRCDIR=src

CC=gcc
CFLAGS=-c -std=c99 -march=native -O2 -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE -I$(INCDIR)
LD=gcc
LDFLAGS=-s


LIBS=-pthread

_DEPS = address.h clientlist.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = server.o address.o clientlist.o
OBJ = $(patsubst %,$(OBJDIR)/%,$(_OBJ))
# OBJ=$(OBJDIR)/server.o $(OBJDIR)/address.o $(OBJDIR)/clientlist.o

server: $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) $< -o $@



.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o *~ $(INCDIR)/*~

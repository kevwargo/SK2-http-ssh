IDIR = include
ODIR=obj
SRCDIR=src

CC=gcc
CFLAGS=-c -std=c99 -march=native -O2 -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE -I$(IDIR)
LD=gcc
LDFLAGS=-s


LIBS=

_DEPS = address.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = server.o address.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) $< -o $@

server: $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ $(IDIR)/*~

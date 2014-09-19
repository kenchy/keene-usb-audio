ODIR=obj

CC=gcc
CFLAGS=-Wall -Wpedantic
LIBS=-lusb -lm

BIN=keene
OBJ = keene.o

_OBJ = $(patsubst %,$(ODIR)/%,$(OBJ))

ALL: keene

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(BIN): $(_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o keene

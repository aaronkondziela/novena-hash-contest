CC=gcc
CFLAGS=-O3 -Wall -m64 -march=corei7
LIBS=-lcrypto -ljson-c

DEPS = novena.h
OBJ = novena.o

all: novena gen

novena: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

gen: gen.o
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

gen.o: gen.c
	$(CC) -c -o $@ $< -g $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< -g $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o
	rm -f novena
	rm -f gen

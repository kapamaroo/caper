CC=gcc
CFLAGS=-Wall -g
#CFLAGS=-Wall -O3 -march=native
DEPS = parser.h datatypes.h analysis.h hash.h
OBJ = main.o parser.o analysis.o hash.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

caper: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f *.o *~ core caper

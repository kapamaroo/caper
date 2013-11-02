CC=gcc
CFLAGS=-Wall -g -I.
DEPS = parser.h datatypes.h
OBJ = main.o parser.o analysis.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

caper: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f *.o *~ core caper

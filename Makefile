CC=gcc
#CFLAGS=-Wall -g
CFLAGS=-Wall -O3 -march=native
DEPS = parser.h datatypes.h analysis.h
OBJ = main.o parser.o analysis.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

caper: $(OBJ)
	gcc -o $@ $^ $(CFLAGS_RELEASE)


.PHONY: clean

clean:
	rm -f *.o *~ core caper

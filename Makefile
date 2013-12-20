CC=gcc
CFLAGS=-Wall -lgsl -lgslcblas -lm -g -UNDEBUG
#CFLAGS=-Wall -lgsl -lgslcblas -lm -O3 -march=native -DNDEBUG
DEPS = parser.h datatypes.h analysis.h hash.h
OBJ = main.o parser.o analysis.o hash.o csparse/csparse.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

caper: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f *.o *~ core caper

archive:
	git archive --format=zip master -o caper.zip

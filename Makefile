CC = gcc
CCFLAGS = -Wall -O3 -funroll-loops -ljpeg -lm
CFLAGS = -Wall -O3 -funroll-loops
PROGRAMS = jpegtochat

all: $(PROGRAMS)

jpegtochat: jpegtochat.o libptoc.o
	$(CC) $(CCFLAGS) -o $@ $^

clean:
	rm -f *.o *~

wipe: clean
	rm -f $(PROGRAMS)

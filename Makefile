CC = gcc
CCFLAGS = -ljpeg -lm
CFLAGS = -ljpeg -lm
PROGRAMS = jpegtochat

all: $(PROGRAMS)

jpegtochat: jpegtochat.o libptoc.o
	$(CC) -o $@ $^ $(CCFLAGS)

clean:
	rm -f *.o *~

wipe: clean
	rm -f $(PROGRAMS)

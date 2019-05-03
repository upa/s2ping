
CC = gcc
INCLUDE := -I./
LDFLAGS := -pthread
CFLAGS := -g -Wall $(INCLUDE)

PROGNAME = s2pingd

all: $(PROGNAME)

.c.o:
	$(CC) $< -o $@

clean:
	rm -rf *.o
	rm -rf $(PROGNAME)

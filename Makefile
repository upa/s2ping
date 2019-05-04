
CC = gcc
INCLUDE := -I./
LDFLAGS := -pthread
CFLAGS := -g -Wall $(INCLUDE)

PROGNAME = s2pingd s2ping

all: $(PROGNAME)

.c.o:
	$(CC) $< -o $@

clean:
	rm -rf *.o
	rm -rf $(PROGNAME)

install: all
	install -m 755 -D ./s2pingd /usr/local/sbin/
	install -m 755 -D ./s2ping /usr/local/sbin/
	chmod a+s /usr/local/sbin/s2ping
	install -m 644 -D ./systemd/s2pingd /etc/default/
	install -m 644 -D ./systemd/s2pingd.service /lib/systemd/system/
